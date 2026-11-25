#include "domain/finance/value_objects/expenseamounts.h"

#include "domain/common/value_objects/moneyamount.h"

namespace domain {

std::optional<ExpenseAmounts> ExpenseAmounts::fromSubtotal(const QString &subtotal)
{
    const std::optional<qint64> cents = MoneyAmount::toCents(subtotal);
    if (!cents)
        return std::nullopt;
    return ExpenseAmounts(*cents);
}

ExpenseAmounts::ExpenseAmounts(qint64 subtotalCents)
    : m_subtotalCents(subtotalCents)
    // Enteros no negativos: sumar 50 antes de dividir redondea la mitad hacia
    // arriba.
    , m_ivaCents((subtotalCents * kIvaPercent + 50) / 100)
{
}

qint64 ExpenseAmounts::subtotalCents() const
{
    return m_subtotalCents;
}

qint64 ExpenseAmounts::ivaCents() const
{
    return m_ivaCents;
}

qint64 ExpenseAmounts::totalCents() const
{
    return m_subtotalCents + m_ivaCents;
}

QString ExpenseAmounts::subtotal() const
{
    return MoneyAmount::fromCents(subtotalCents());
}

QString ExpenseAmounts::iva() const
{
    return MoneyAmount::fromCents(ivaCents());
}

QString ExpenseAmounts::total() const
{
    return MoneyAmount::fromCents(totalCents());
}

} // namespace domain
