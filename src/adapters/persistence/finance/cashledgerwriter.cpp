#include "adapters/persistence/finance/cashledgerwriter.h"

#include "domain/finance/rules/cashflowrules.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cashledger {

namespace {

QVariant nullable(const std::optional<int> &value)
{
    return value ? QVariant(*value) : QVariant(QMetaType(QMetaType::Int));
}

ApplyOutcome failed(const QString &message)
{
    ApplyOutcome outcome;
    outcome.status = ApplyOutcome::Status::Failed;
    outcome.errorMessage = message;
    return outcome;
}

} // namespace

std::optional<qint64> balanceOf(QSqlDatabase &db, domain::CashAccount account, QString *error)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT CAST(ROUND(balance * 100) AS BIGINT) FROM cash_accounts "
                                 "WHERE account = :account"));
    query.bindValue(QStringLiteral(":account"), domain::toDbString(account));
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next()) {
        if (error)
            *error = QStringLiteral("No existe la cuenta %1.").arg(domain::displayLabel(account));
        return std::nullopt;
    }
    return query.value(0).toLongLong();
}

ApplyOutcome applyMovements(QSqlDatabase &db, const QList<domain::CashMovement> &movements,
                            const References &references)
{
    for (const domain::CashMovement &movement : domain::CashFlowRules::inApplyOrder(movements)) {
        const QString account = domain::toDbString(movement.account());
        QSqlQuery update(db);
        // En centésimos y entero: "balance * 100 + :monto >= 0" es exacto.
        update.prepare(QStringLiteral(
            "UPDATE cash_accounts SET balance = balance + CAST(:amount AS NUMERIC) / 100, "
            "       updated_at = CURRENT_TIMESTAMP "
            " WHERE account = :account AND balance * 100 + :amount >= 0"));
        update.bindValue(QStringLiteral(":amount"), movement.hundredths());
        update.bindValue(QStringLiteral(":account"), account);
        if (!update.exec())
            return failed(update.lastError().text());
        if (update.numRowsAffected() != 1) {
            QString error;
            const std::optional<qint64> available = balanceOf(db, movement.account(), &error);
            if (!available)
                return failed(error);
            ApplyOutcome outcome;
            outcome.status = ApplyOutcome::Status::Insufficient;
            outcome.account = movement.account();
            outcome.available = *available;
            return outcome;
        }

        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO cash_movements (account, amount, kind, concept, occurred_on, user_id, "
            "                            transfer_id, vehicle_folio, sale_folio) "
            "VALUES (:account, CAST(:amount AS NUMERIC) / 100, :kind, :concept, :date, :user, "
            "        :transfer, :vehicle, :sale)"));
        insert.bindValue(QStringLiteral(":account"), account);
        insert.bindValue(QStringLiteral(":amount"), movement.hundredths());
        insert.bindValue(QStringLiteral(":kind"), domain::toDbString(movement.kind()));
        insert.bindValue(QStringLiteral(":concept"), movement.concept());
        insert.bindValue(QStringLiteral(":date"),
                         movement.date().isValid() ? movement.date() : QDate::currentDate());
        insert.bindValue(QStringLiteral(":user"), nullable(references.userId));
        insert.bindValue(QStringLiteral(":transfer"), nullable(references.transferId));
        insert.bindValue(QStringLiteral(":vehicle"), nullable(references.vehicleFolio));
        insert.bindValue(QStringLiteral(":sale"), nullable(references.saleFolio));
        if (!insert.exec())
            return failed(insert.lastError().text());
    }
    ApplyOutcome outcome;
    outcome.status = ApplyOutcome::Status::Applied;
    return outcome;
}

std::optional<int> nextTransferId(QSqlDatabase &db, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT nextval('cash_transfer_seq')")) || !query.next()) {
        if (error)
            *error = query.lastError().text();
        return std::nullopt;
    }
    return query.value(0).toInt();
}

} // namespace cashledger
