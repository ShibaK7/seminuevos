#include "domain/finance/rules/cashflowrules.h"

#include <QMap>

#include <algorithm>

namespace domain {

namespace {

// Lo que una compra deja en cada cuenta (negativo: salió).
QMap<CashAccount, qint64> effectOf(const PurchaseTerms &terms)
{
    QMap<CashAccount, qint64> effect;
    if (terms.acquisitionType == AcquisitionType::Adquisicion
        && terms.paymentType == PaymentType::Contado && terms.priceHundredths > 0)
        effect.insert(accountFor(terms.paymentMethod), -terms.priceHundredths);
    return effect;
}

QList<CashMovement> outflowOf(PaymentMethod method, qint64 hundredths, CashMovementKind kind,
                              const QString &concept, const QDate &date)
{
    if (hundredths <= 0)
        return {};
    return {CashMovement::outflow(accountFor(method), hundredths, kind, concept, date)};
}

QList<CashMovement> inflowOf(PaymentMethod method, qint64 hundredths, CashMovementKind kind,
                             const QString &concept, const QDate &date)
{
    if (hundredths <= 0)
        return {};
    return {CashMovement::inflow(accountFor(method), hundredths, kind, concept, date)};
}

} // namespace

bool PurchaseTerms::operator==(const PurchaseTerms &other) const
{
    return acquisitionType == other.acquisitionType && paymentType == other.paymentType
           && paymentMethod == other.paymentMethod && priceHundredths == other.priceHundredths;
}

QList<CashMovement> CashFlowRules::forPurchase(const PurchaseTerms &terms, const QString &concept,
                                               const QDate &date)
{
    QList<CashMovement> movements;
    const QMap<CashAccount, qint64> effect = effectOf(terms);
    for (auto it = effect.cbegin(); it != effect.cend(); ++it)
        movements << CashMovement::outflow(it.key(), -it.value(), CashMovementKind::Purchase, concept, date);
    return movements;
}

QList<CashMovement> CashFlowRules::forPurchaseCorrection(const PurchaseTerms &before,
                                                         const PurchaseTerms &after,
                                                         const QString &concept, const QDate &date)
{
    if (before == after)
        return {};
    // Lo que debe quedar menos lo que ya quedó, cuenta por cuenta.
    QMap<CashAccount, qint64> delta = effectOf(after);
    const QMap<CashAccount, qint64> previous = effectOf(before);
    for (auto it = previous.cbegin(); it != previous.cend(); ++it)
        delta[it.key()] -= it.value();

    QList<CashMovement> movements;
    for (const CashAccount account : allCashAccounts()) {
        const qint64 amount = delta.value(account);
        if (amount > 0)
            movements << CashMovement::inflow(account, amount, CashMovementKind::PurchaseCorrection,
                                              concept, date);
        else if (amount < 0)
            movements << CashMovement::outflow(account, -amount, CashMovementKind::PurchaseCorrection,
                                               concept, date);
    }
    return movements;
}

QList<CashMovement> CashFlowRules::forCashSale(PaymentMethod method, qint64 totalHundredths,
                                               const QString &concept, const QDate &date)
{
    return inflowOf(method, totalHundredths, CashMovementKind::Sale, concept, date);
}

QList<CashMovement> CashFlowRules::forCreditSale(PaymentMethod downPaymentMethod,
                                                 qint64 downPaymentHundredths,
                                                 const QString &concept, const QDate &date)
{
    return inflowOf(downPaymentMethod, downPaymentHundredths, CashMovementKind::DownPayment, concept,
                    date);
}

QList<CashMovement> CashFlowRules::forMaintenance(PaymentMethod method, qint64 costHundredths,
                                                  const QString &concept, const QDate &date)
{
    return outflowOf(method, costHundredths, CashMovementKind::Maintenance, concept, date);
}

QList<CashMovement> CashFlowRules::forExpense(PaymentMethod method, qint64 totalHundredths,
                                              const QString &concept, const QDate &date)
{
    return outflowOf(method, totalHundredths, CashMovementKind::Expense, concept, date);
}

QList<CashMovement> CashFlowRules::inApplyOrder(QList<CashMovement> movements)
{
    std::stable_sort(movements.begin(), movements.end(),
                     [](const CashMovement &a, const CashMovement &b) {
                         if (a.account() != b.account())
                             return a.account() < b.account();
                         return a.isInflow() && !b.isInflow();
                     });
    return movements;
}

} // namespace domain
