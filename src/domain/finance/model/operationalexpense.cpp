#include "domain/finance/model/operationalexpense.h"

#include "domain/common/value_objects/moneyamount.h"

namespace domain {

namespace {

// Por qué el subtotal no sirve, o vacío si sirve.
QString subtotalProblem(const QString &subtotal, const std::optional<ExpenseAmounts> &amounts)
{
    using Problem = MoneyAmount::Problem;
    switch (MoneyAmount::check(subtotal)) {
    case Problem::None:
        break;
    case Problem::Empty:
        return QStringLiteral("Captura el subtotal.");
    case Problem::Negative:
        return QStringLiteral("El subtotal no puede ser negativo.");
    case Problem::NotANumber:
        return QStringLiteral("El subtotal debe ser un número, por ejemplo 1500.50.");
    case Problem::TooManyDecimals:
        return QStringLiteral("El subtotal admite hasta %1 decimales.")
            .arg(MoneyAmount::kDecimals);
    case Problem::TooManyIntegerDigits:
        return QStringLiteral("El subtotal admite hasta %1 dígitos antes del punto.")
            .arg(MoneyAmount::kIntegerDigits);
    }
    // El subtotal cabe, pero con el IVA el total puede pasarse de la columna
    // total_amount NUMERIC(12, 2).
    if (amounts && amounts->totalCents() >= MoneyAmount::kCentsLimit)
        return QStringLiteral("Con el IVA, el total no cabe: captura un subtotal menor.");
    return QString();
}

} // namespace

OperationalExpense::OperationalExpense(const QDate &date, int typeId, bool hasInvoice,
                                       const QString &invoiceNumber, const QString &subtotal,
                                       std::optional<PaymentMethod> paymentMethod)
    : m_date(date)
    , m_typeId(typeId)
    , m_hasInvoice(hasInvoice)
    , m_invoiceNumber(hasInvoice ? invoiceNumber.trimmed() : QString())
    , m_subtotal(subtotal.trimmed())
    , m_amounts(ExpenseAmounts::fromSubtotal(m_subtotal))
    , m_paymentMethod(paymentMethod)
{
}

const QDate &OperationalExpense::date() const
{
    return m_date;
}

int OperationalExpense::typeId() const
{
    return m_typeId;
}

bool OperationalExpense::hasInvoice() const
{
    return m_hasInvoice;
}

const QString &OperationalExpense::invoiceNumber() const
{
    return m_invoiceNumber;
}

const std::optional<ExpenseAmounts> &OperationalExpense::amounts() const
{
    return m_amounts;
}

std::optional<PaymentMethod> OperationalExpense::paymentMethod() const
{
    return m_paymentMethod;
}

ValidationResult OperationalExpense::validate() const
{
    ValidationResult result;
    if (!m_date.isValid())
        result.addError(QStringLiteral("expenseDate"), QStringLiteral("Elige la fecha del gasto."));
    // Los ids de operational_expense_cat salen de un SERIAL: empiezan en 1.
    if (m_typeId <= 0)
        result.addError(QStringLiteral("expenseType"),
                        QStringLiteral("Selecciona el tipo de gasto."));
    result.merge(validateInvoice(m_hasInvoice, m_invoiceNumber));
    const QString problem = subtotalProblem(m_subtotal, m_amounts);
    if (!problem.isEmpty())
        result.addError(QStringLiteral("subtotal"), problem);
    // payment_method es NOT NULL: no hay un valor por omisión que suponer.
    if (!m_paymentMethod)
        result.addError(QStringLiteral("paymentMethod"),
                        QStringLiteral("Selecciona el método de pago."));
    return result;
}

ValidationResult OperationalExpense::validateInvoice(bool hasInvoice, const QString &invoiceNumber)
{
    ValidationResult result;
    if (!hasInvoice)
        return result;
    const QString number = invoiceNumber.trimmed();
    if (number.isEmpty())
        result.addError(QStringLiteral("invoiceNumber"),
                        QStringLiteral("Captura el número de factura."));
    else if (number.size() > kInvoiceNumberMaxLength)
        result.addError(QStringLiteral("invoiceNumber"),
                        QStringLiteral("El número de factura admite hasta %1 caracteres.")
                            .arg(kInvoiceNumberMaxLength));
    return result;
}

} // namespace domain
