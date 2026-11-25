#ifndef DOMAIN_FINANCE_MODEL_OPERATIONALEXPENSE_H
#define DOMAIN_FINANCE_MODEL_OPERATIONALEXPENSE_H

#include "domain/common/value_objects/enums.h"
#include "domain/common/value_objects/validationresult.h"
#include "domain/finance/value_objects/expenseamounts.h"

#include <QDate>
#include <QString>

#include <optional>

namespace domain {

// Un gasto operativo de la agencia (renta, luz, papelería...). Mapea un
// renglón de operational_expenses; el folio lo pone la base al guardarlo.
//
// El subtotal llega como el texto que se capturó: esta clase decide si es un
// importe válido y de él saca el IVA y el total (ExpenseAmounts). Nadie más
// los calcula, así que lo guardado siempre cuadra.
class OperationalExpense
{
public:
    // invoice_number VARCHAR(50).
    static constexpr int kInvoiceNumberMaxLength = 50;

    // typeId es el id de operational_expense_cat; -1 si no se eligió.
    // paymentMethod es nullopt si no se eligió. Sin factura, el número se
    // descarta.
    OperationalExpense(const QDate &date, int typeId, bool hasInvoice,
                       const QString &invoiceNumber, const QString &subtotal,
                       std::optional<PaymentMethod> paymentMethod);

    const QDate &date() const;
    int typeId() const;
    bool hasInvoice() const;
    // Sin espacios alrededor; vacío si no tiene factura.
    const QString &invoiceNumber() const;
    // Siempre tiene valor si validate() no reporta error en "subtotal".
    const std::optional<ExpenseAmounts> &amounts() const;
    // Siempre tiene valor si validate() no reporta error en "paymentMethod".
    std::optional<PaymentMethod> paymentMethod() const;

    // Campos: "expenseDate", "expenseType", "invoiceNumber", "subtotal" y
    // "paymentMethod".
    ValidationResult validate() const;

    // La regla de la factura sola, para corregirla en un gasto ya guardado
    // sin rearmar el renglón. Con factura, el número es obligatorio. Campo
    // "invoiceNumber".
    static ValidationResult validateInvoice(bool hasInvoice, const QString &invoiceNumber);

private:
    QDate m_date;
    int m_typeId;
    bool m_hasInvoice;
    QString m_invoiceNumber;
    QString m_subtotal;
    std::optional<ExpenseAmounts> m_amounts;
    std::optional<PaymentMethod> m_paymentMethod;
};

} // namespace domain

#endif // DOMAIN_FINANCE_MODEL_OPERATIONALEXPENSE_H
