#include "application/finance/services/cashledgerservice.h"

#include "domain/finance/rules/cashmovementpolicy.h"
#include "domain/settings/rules/settingsaccess.h"

namespace application {

namespace {

bool canManage(const SessionDto &actor)
{
    return domain::SettingsAccess(actor.role).canManageSystemParameters();
}

CashWriteResult withStatus(CashWriteResult::Status status)
{
    CashWriteResult result;
    result.status = status;
    return result;
}

CashWriteResult rejected(const domain::ValidationResult &validation)
{
    CashWriteResult result;
    result.status = CashWriteResult::Status::Rejected;
    result.validation = validation;
    return result;
}

qint64 balanceOf(const QList<CashBalanceDto> &balances, domain::CashAccount account)
{
    for (const CashBalanceDto &balance : balances) {
        if (balance.account == account)
            return balance.hundredths;
    }
    return 0;
}

} // namespace

CashLedgerService::CashLedgerService(CashLedger &ledger)
    : m_ledger(ledger)
{
}

CashOverviewResult CashLedgerService::overview(const SessionDto &actor,
                                               std::optional<domain::CashAccount> account) const
{
    using Status = CashOverviewResult::Status;
    CashOverviewResult result;
    if (!canManage(actor)) {
        result.status = Status::Denied;
        return result;
    }
    QString error;
    const QList<CashBalanceDto> stored = m_ledger.balances(&error);
    if (error.isEmpty())
        result.movements = m_ledger.recentMovements(account, kMovementsShown, &error);
    if (!error.isEmpty()) {
        result.status = Status::Failed;
        result.errorMessage = error;
        result.movements.clear();
        return result;
    }
    // Siempre las tres y en orden, aunque falte un renglón.
    for (const domain::CashAccount cashAccount : domain::allCashAccounts()) {
        CashBalanceDto dto;
        dto.account = cashAccount;
        dto.opened = cashAccount == domain::CashAccount::Safe;
        for (const CashBalanceDto &row : stored) {
            if (row.account == cashAccount)
                dto = row;
        }
        result.balances << dto;
    }
    result.status = Status::Loaded;
    return result;
}

std::optional<QList<CashBalanceDto>> CashLedgerService::readBalances(CashWriteResult &failed) const
{
    QString error;
    const QList<CashBalanceDto> balances = m_ledger.balances(&error);
    if (!error.isEmpty()) {
        failed = withStatus(CashWriteResult::Status::Failed);
        failed.errorMessage = error;
        return std::nullopt;
    }
    return balances;
}

CashWriteResult CashLedgerService::openBalances(const OpeningBalancesDto &openings,
                                                const SessionDto &actor) const
{
    if (!canManage(actor))
        return withStatus(CashWriteResult::Status::Denied);

    domain::ValidationResult validation;
    domain::CashMovementPolicy::checkOpening(domain::CashAccount::PettyCash,
                                             openings.pettyCashHundredths, validation);
    domain::CashMovementPolicy::checkOpening(domain::CashAccount::Bank, openings.bankHundredths,
                                             validation);
    if (!validation.isValid())
        return rejected(validation);

    CashWriteResult failed;
    const auto balances = readBalances(failed);
    if (!balances)
        return failed;
    for (const CashBalanceDto &balance : *balances) {
        if (balance.account != domain::CashAccount::Safe && balance.opened)
            return withStatus(CashWriteResult::Status::Conflict);
    }
    return fromOutcome(m_ledger.open(openings, actor.userId));
}

CashWriteResult CashLedgerService::depositToSafe(const SafeDepositDto &deposit,
                                                 const SessionDto &actor) const
{
    if (!canManage(actor))
        return withStatus(CashWriteResult::Status::Denied);
    CashWriteResult failed;
    const auto balances = readBalances(failed);
    if (!balances)
        return failed;

    domain::ValidationResult validation;
    domain::CashMovementPolicy::checkSafeDeposit(deposit.source, deposit.hundredths,
                                                 balanceOf(*balances, deposit.source), validation);
    if (!validation.isValid())
        return rejected(validation);
    return fromOutcome(m_ledger.depositToSafe(deposit, actor.userId));
}

CashWriteResult CashLedgerService::withdrawFromSafe(const SafeWithdrawalDto &withdrawal,
                                                    const SessionDto &actor, const QDate &today) const
{
    if (!canManage(actor))
        return withStatus(CashWriteResult::Status::Denied);
    CashWriteResult failed;
    const auto balances = readBalances(failed);
    if (!balances)
        return failed;

    domain::ValidationResult validation;
    domain::CashMovementPolicy::checkSafeWithdrawal(
        withdrawal.date, today, withdrawal.concept, withdrawal.hundredths,
        balanceOf(*balances, domain::CashAccount::Safe), validation);
    if (!validation.isValid())
        return rejected(validation);

    SafeWithdrawalDto trimmed = withdrawal;
    trimmed.concept = withdrawal.concept.trimmed();
    return fromOutcome(m_ledger.withdrawFromSafe(trimmed, actor.userId));
}

CashWriteResult CashLedgerService::fromOutcome(const CashLedger::WriteOutcome &outcome) const
{
    using Status = CashLedger::WriteOutcome::Status;
    switch (outcome.status) {
    case Status::Ok:
        return withStatus(CashWriteResult::Status::Saved);
    case Status::Insufficient: {
        // Otra sesión gastó ese dinero entre leer y guardar.
        domain::ValidationResult validation;
        validation.addError(domain::CashMovementPolicy::amountField(),
                            domain::CashMovementPolicy::insufficientFundsMessage(outcome.account,
                                                                                 outcome.available));
        return rejected(validation);
    }
    case Status::AlreadyOpened:
        return withStatus(CashWriteResult::Status::Conflict);
    case Status::ActorNotAllowed:
        return withStatus(CashWriteResult::Status::Denied);
    case Status::Failed:
        break;
    }
    CashWriteResult result = withStatus(CashWriteResult::Status::Failed);
    result.errorMessage = outcome.errorMessage;
    return result;
}

} // namespace application
