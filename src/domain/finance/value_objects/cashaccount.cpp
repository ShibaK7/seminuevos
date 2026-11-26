#include "domain/finance/value_objects/cashaccount.h"

namespace domain {

QList<CashAccount> allCashAccounts()
{
    return {CashAccount::PettyCash, CashAccount::Bank, CashAccount::Safe};
}

QString toDbString(CashAccount account)
{
    switch (account) {
    case CashAccount::PettyCash:
        return QStringLiteral("Caja Chica");
    case CashAccount::Bank:
        return QStringLiteral("Banco");
    case CashAccount::Safe:
        return QStringLiteral("Caja Fuerte");
    }
    return QString();
}

std::optional<CashAccount> cashAccountFromDb(const QString &raw)
{
    for (const CashAccount account : allCashAccounts()) {
        if (toDbString(account) == raw)
            return account;
    }
    return std::nullopt;
}

QString displayLabel(CashAccount account)
{
    switch (account) {
    case CashAccount::PettyCash:
        return QStringLiteral("Caja Chica");
    case CashAccount::Bank:
        return QStringLiteral("Cantidad Banco");
    case CashAccount::Safe:
        return QStringLiteral("Caja Fuerte");
    }
    return QString();
}

CashAccount accountFor(PaymentMethod method)
{
    return method == PaymentMethod::Efectivo ? CashAccount::PettyCash : CashAccount::Bank;
}

} // namespace domain
