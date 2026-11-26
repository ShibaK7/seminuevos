#include "adapters/persistence/finance/sqlcashledger.h"

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/finance/cashledgerwriter.h"
#include "adapters/persistence/settings/activeadministrator.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

using WriteOutcome = application::CashLedger::WriteOutcome;
using Status = WriteOutcome::Status;

WriteOutcome withStatus(Status status)
{
    WriteOutcome outcome;
    outcome.status = status;
    return outcome;
}

WriteOutcome failed(const QString &message)
{
    WriteOutcome outcome;
    outcome.status = Status::Failed;
    outcome.errorMessage = message;
    return outcome;
}

WriteOutcome fromApply(const cashledger::ApplyOutcome &applied)
{
    switch (applied.status) {
    case cashledger::ApplyOutcome::Status::Applied:
        return withStatus(Status::Ok);
    case cashledger::ApplyOutcome::Status::Insufficient: {
        WriteOutcome outcome = withStatus(Status::Insufficient);
        outcome.account = applied.account;
        outcome.available = applied.available;
        return outcome;
    }
    case cashledger::ApplyOutcome::Status::Failed:
        break;
    }
    return failed(applied.errorMessage);
}

} // namespace

SqlCashLedger::SqlCashLedger(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::CashBalanceDto> SqlCashLedger::balances(QString *error)
{
    QList<application::CashBalanceDto> balances;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        if (!db.isOpen()
            || !query.exec(QStringLiteral("SELECT account, CAST(ROUND(balance * 100) AS BIGINT), opened, "
                                          "updated_at FROM cash_accounts"))) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                const std::optional<domain::CashAccount> account =
                    domain::cashAccountFromDb(query.value(0).toString());
                if (!account)
                    continue;
                application::CashBalanceDto dto;
                dto.account = *account;
                dto.hundredths = query.value(1).toLongLong();
                dto.opened = query.value(2).toBool();
                dto.updatedAt = query.value(3).toDateTime();
                balances << dto;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        return {};
    }
    if (error)
        error->clear();
    return balances;
}

QList<application::CashMovementDto> SqlCashLedger::recentMovements(
    std::optional<domain::CashAccount> account, int limit, QString *error)
{
    QList<application::CashMovementDto> movements;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT m.id, m.account, CAST(ROUND(m.amount * 100) AS BIGINT), m.kind, m.concept, "
            "       m.occurred_on, m.created_at, u.display_name "
            "  FROM cash_movements m LEFT JOIN users u ON u.id = m.user_id "
            " WHERE (:all OR m.account = :account) "
            " ORDER BY m.created_at DESC, m.id DESC LIMIT :limit"));
        query.bindValue(QStringLiteral(":all"), !account.has_value());
        query.bindValue(QStringLiteral(":account"),
                        account ? domain::toDbString(*account) : QString());
        query.bindValue(QStringLiteral(":limit"), limit);
        if (!db.isOpen() || !query.exec()) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                const auto movementAccount = domain::cashAccountFromDb(query.value(1).toString());
                const auto kind = domain::cashMovementKindFromDb(query.value(3).toString());
                if (!movementAccount || !kind)
                    continue;
                application::CashMovementDto dto;
                dto.id = query.value(0).toInt();
                dto.account = *movementAccount;
                dto.hundredths = query.value(2).toLongLong();
                dto.kind = *kind;
                dto.concept = query.value(4).toString();
                dto.date = query.value(5).toDate();
                dto.createdAt = query.value(6).toDateTime();
                dto.userName = query.value(7).toString();
                movements << dto;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        return {};
    }
    if (error)
        error->clear();
    return movements;
}

WriteOutcome SqlCashLedger::inAdministratorTransaction(
    int actorId, const std::function<WriteOutcome(QSqlDatabase &)> &write)
{
    WriteOutcome result;
    bool databaseFailed = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            databaseFailed = true;
            result = failed(QStringLiteral("No se pudo conectar con la base de datos: %1")
                                .arg(db.lastError().text()));
        } else if (!db.transaction()) {
            databaseFailed = true;
            result = failed(QStringLiteral("No se pudo iniciar la transacción: %1")
                                .arg(db.lastError().text()));
        } else {
            QString error;
            if (!isActiveAdministrator(db, actorId, &error))
                result = error.isEmpty() ? withStatus(Status::ActorNotAllowed) : failed(error);
            else
                result = write(db);

            if (result.status != Status::Ok) {
                db.rollback();
            } else if (!db.commit()) {
                result = failed(QStringLiteral("No se pudo confirmar la transacción: %1")
                                    .arg(db.lastError().text()));
                db.rollback();
            }
            databaseFailed = result.status == Status::Failed;
        }
    }
    // Con la conexión ya soltada, una que falló se descarta para que el
    // reintento abra otra.
    if (databaseFailed)
        m_pool.discardThreadConnection();
    return result;
}

WriteOutcome SqlCashLedger::open(const application::OpeningBalancesDto &openings, int actorId)
{
    return inAdministratorTransaction(actorId, [&openings](QSqlDatabase &db) {
        const QList<std::pair<domain::CashAccount, qint64>> accounts = {
            {domain::CashAccount::PettyCash, openings.pettyCashHundredths},
            {domain::CashAccount::Bank, openings.bankHundredths},
        };
        QList<domain::CashMovement> movements;
        for (const auto &[account, hundredths] : accounts) {
            // Solo si sigue cerrada: si otro administrador la abrió mientras
            // tanto, no se suma dos veces.
            QSqlQuery mark(db);
            mark.prepare(QStringLiteral("UPDATE cash_accounts SET opened = TRUE, updated_at = CURRENT_TIMESTAMP "
                                        " WHERE account = :account AND NOT opened"));
            mark.bindValue(QStringLiteral(":account"), domain::toDbString(account));
            if (!mark.exec())
                return failed(mark.lastError().text());
            if (mark.numRowsAffected() != 1)
                return withStatus(Status::AlreadyOpened);
            if (hundredths > 0)
                movements << domain::CashMovement::inflow(account, hundredths,
                                                          domain::CashMovementKind::Opening,
                                                          QStringLiteral("Saldo inicial"),
                                                          QDate::currentDate());
        }
        cashledger::References references;
        return fromApply(cashledger::applyMovements(db, movements, references));
    });
}

WriteOutcome SqlCashLedger::depositToSafe(const application::SafeDepositDto &deposit, int actorId)
{
    return inAdministratorTransaction(actorId, [&deposit, actorId](QSqlDatabase &db) {
        QString error;
        const std::optional<int> transferId = cashledger::nextTransferId(db, &error);
        if (!transferId)
            return failed(error);
        const QDate today = QDate::currentDate();
        const QList<domain::CashMovement> movements = {
            domain::CashMovement::outflow(deposit.source, deposit.hundredths,
                                          domain::CashMovementKind::Transfer,
                                          QStringLiteral("Ingreso a Caja Fuerte"), today),
            domain::CashMovement::inflow(domain::CashAccount::Safe, deposit.hundredths,
                                         domain::CashMovementKind::Transfer,
                                         QStringLiteral("Ingreso desde %1")
                                             .arg(domain::displayLabel(deposit.source)),
                                         today),
        };
        cashledger::References references;
        references.userId = actorId;
        references.transferId = transferId;
        return fromApply(cashledger::applyMovements(db, movements, references));
    });
}

WriteOutcome SqlCashLedger::withdrawFromSafe(const application::SafeWithdrawalDto &withdrawal,
                                             int actorId)
{
    return inAdministratorTransaction(actorId, [&withdrawal, actorId](QSqlDatabase &db) {
        cashledger::References references;
        references.userId = actorId;
        return fromApply(cashledger::applyMovements(
            db,
            {domain::CashMovement::outflow(domain::CashAccount::Safe, withdrawal.hundredths,
                                           domain::CashMovementKind::Withdrawal, withdrawal.concept,
                                           withdrawal.date)},
            references));
    });
}
