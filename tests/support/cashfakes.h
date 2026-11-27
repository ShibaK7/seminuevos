#ifndef TESTS_SUPPORT_CASHFAKES_H
#define TESTS_SUPPORT_CASHFAKES_H

// Las cuentas de dinero en memoria (Caja Chica, Cantidad Banco y Caja
// Fuerte). Aparte de fakes.h para que las pruebas de otros módulos no
// dependan de ellas.

#include "application/finance/ports/cashledger.h"

#include <QMap>
#include <QSet>

namespace fakes {

// Aplica los movimientos como el adaptador: todo o nada, y nunca deja una
// cuenta en negativo.
class InMemoryCashLedger final : public application::CashLedger
{
public:
    static constexpr int kAdministratorId = 1;

    QMap<domain::CashAccount, application::CashBalanceDto> accounts;
    QList<application::CashMovementDto> movements;
    QSet<int> activeAdministrators{kAdministratorId};
    // Si no está vacío, todo "falla" con este error.
    QString failure;
    int writes = 0;
    // Lo que "gasta" otra sesión justo antes de que se aplique una escritura
    // (para probar la falta de saldo que el servicio no vio al leer).
    QMap<domain::CashAccount, qint64> spentMeanwhile;
    QDateTime now = QDateTime(QDate(2026, 11, 26), QTime(10, 0));

    InMemoryCashLedger()
    {
        for (const domain::CashAccount account : domain::allCashAccounts()) {
            application::CashBalanceDto balance;
            balance.account = account;
            balance.opened = account == domain::CashAccount::Safe;
            accounts.insert(account, balance);
        }
    }

    // Deja la cuenta abierta con este saldo.
    void setBalance(domain::CashAccount account, qint64 hundredths)
    {
        accounts[account].hundredths = hundredths;
        accounts[account].opened = true;
    }

    qint64 balance(domain::CashAccount account) const { return accounts.value(account).hundredths; }

    QList<application::CashBalanceDto> balances(QString *error) override
    {
        if (error)
            *error = failure;
        return failure.isEmpty() ? accounts.values() : QList<application::CashBalanceDto>();
    }

    QList<application::CashMovementDto> recentMovements(std::optional<domain::CashAccount> account,
                                                        int limit, QString *error) override
    {
        if (error)
            *error = failure;
        QList<application::CashMovementDto> result;
        for (auto it = movements.crbegin(); it != movements.crend() && result.size() < limit; ++it) {
            if (!account || it->account == *account)
                result << *it;
        }
        return result;
    }

    WriteOutcome open(const application::OpeningBalancesDto &openings, int actorId) override
    {
        WriteOutcome outcome = check(actorId);
        if (outcome.status != WriteOutcome::Status::Ok)
            return outcome;
        if (accounts[domain::CashAccount::PettyCash].opened || accounts[domain::CashAccount::Bank].opened)
            return status(WriteOutcome::Status::AlreadyOpened);
        accounts[domain::CashAccount::PettyCash].opened = true;
        accounts[domain::CashAccount::Bank].opened = true;
        // Como el adaptador: el saldo queda en lo capturado.
        QList<domain::CashMovement> opening;
        const QList<std::pair<domain::CashAccount, qint64>> captured = {
            {domain::CashAccount::PettyCash, openings.pettyCashHundredths},
            {domain::CashAccount::Bank, openings.bankHundredths}};
        for (const auto &[account, hundredths] : captured) {
            const qint64 difference = hundredths - accounts[account].hundredths;
            if (difference > 0)
                opening << domain::CashMovement::inflow(account, difference, domain::CashMovementKind::Opening,
                                                        QStringLiteral("Saldo inicial"), now.date());
            else if (difference < 0)
                opening << domain::CashMovement::outflow(account, -difference, domain::CashMovementKind::Opening,
                                                         QStringLiteral("Saldo inicial"), now.date());
        }
        return apply(opening, actorId);
    }

    WriteOutcome depositToSafe(const application::SafeDepositDto &deposit, int actorId) override
    {
        WriteOutcome outcome = check(actorId);
        if (outcome.status != WriteOutcome::Status::Ok)
            return outcome;
        return apply({domain::CashMovement::outflow(deposit.source, deposit.hundredths,
                                                    domain::CashMovementKind::Transfer,
                                                    QStringLiteral("Ingreso a Caja Fuerte"), now.date()),
                      domain::CashMovement::inflow(domain::CashAccount::Safe, deposit.hundredths,
                                                   domain::CashMovementKind::Transfer,
                                                   QStringLiteral("Ingreso"), now.date())},
                     actorId);
    }

    WriteOutcome withdrawFromSafe(const application::SafeWithdrawalDto &withdrawal, int actorId) override
    {
        WriteOutcome outcome = check(actorId);
        if (outcome.status != WriteOutcome::Status::Ok)
            return outcome;
        return apply({domain::CashMovement::outflow(domain::CashAccount::Safe, withdrawal.hundredths,
                                                    domain::CashMovementKind::Withdrawal, withdrawal.concept,
                                                    withdrawal.date)},
                     actorId);
    }

private:
    static WriteOutcome status(WriteOutcome::Status value)
    {
        WriteOutcome outcome;
        outcome.status = value;
        return outcome;
    }

    WriteOutcome check(int actorId)
    {
        ++writes;
        for (auto it = spentMeanwhile.cbegin(); it != spentMeanwhile.cend(); ++it)
            accounts[it.key()].hundredths -= it.value();
        spentMeanwhile.clear();
        if (!failure.isEmpty()) {
            WriteOutcome outcome = status(WriteOutcome::Status::Failed);
            outcome.errorMessage = failure;
            return outcome;
        }
        if (!activeAdministrators.contains(actorId))
            return status(WriteOutcome::Status::ActorNotAllowed);
        return status(WriteOutcome::Status::Ok);
    }

    WriteOutcome apply(const QList<domain::CashMovement> &toApply, int actorId)
    {
        QMap<domain::CashAccount, application::CashBalanceDto> after = accounts;
        for (const domain::CashMovement &movement : toApply) {
            qint64 &balance = after[movement.account()].hundredths;
            if (balance + movement.hundredths() < 0) {
                WriteOutcome outcome = status(WriteOutcome::Status::Insufficient);
                outcome.account = movement.account();
                outcome.available = balance;
                return outcome;
            }
            balance += movement.hundredths();
            after[movement.account()].updatedAt = now;
        }
        accounts = after;
        for (const domain::CashMovement &movement : toApply) {
            application::CashMovementDto dto;
            dto.id = movements.size() + 1;
            dto.account = movement.account();
            dto.hundredths = movement.hundredths();
            dto.kind = movement.kind();
            dto.concept = movement.concept();
            dto.date = movement.date();
            dto.createdAt = now;
            dto.userName = actorId == kAdministratorId ? QStringLiteral("Admin") : QString();
            movements << dto;
        }
        return status(WriteOutcome::Status::Ok);
    }
};

} // namespace fakes

#endif // TESTS_SUPPORT_CASHFAKES_H
