#include "domain/common/rules/cashpaymentlimit.h"

#include <QLocale>

#include <cmath>

namespace domain {

CashPaymentLimit::CashPaymentLimit(double umaDailyValue)
    : m_umaDailyValue(umaDailyValue)
{
}

bool CashPaymentLimit::isConfigured() const
{
    return m_umaDailyValue > 0.0;
}

double CashPaymentLimit::limit() const
{
    return isConfigured() ? kUmaMultiple * m_umaDailyValue : 0.0;
}

bool CashPaymentLimit::allows(PaymentMethod method, double amount) const
{
    if (method != PaymentMethod::Efectivo)
        return true;
    if (!isConfigured())
        return false;
    // La ley dice "igual o superior": alcanzar el tope ya está prohibido.
    // Se compara en centavos y no en double: con la UMA real, 3210 × 117.31
    // da 376565.10000000003 y el importe 376,565.10 se guarda como
    // 376565.09999999998, así que la comparación directa dejaba pasar un pago
    // exactamente en el tope.
    return std::llround(amount * 100.0) < std::llround(limit() * 100.0);
}

void CashPaymentLimit::check(PaymentMethod method, double amount, ValidationResult &result,
                             const QString &field) const
{
    if (allows(method, amount))
        return;

    if (!isConfigured()) {
        result.addError(field,
                        QStringLiteral("No hay UMA configurada; no se puede validar el tope de "
                                       "pago en efectivo. Usa otro método de pago o configura "
                                       "la UMA."));
        return;
    }

    // Formato de México: miles con coma y dos decimales, como se lee en un
    // contrato o una factura.
    const QString amountText =
        QLocale(QLocale::Spanish, QLocale::Mexico).toString(limit(), 'f', 2);
    result.addError(field,
                    QStringLiteral("El pago en efectivo no puede alcanzar 3,210 UMA ($%1). "
                                   "Cambia el método de pago o ajusta el precio.")
                        .arg(amountText));
}

} // namespace domain
