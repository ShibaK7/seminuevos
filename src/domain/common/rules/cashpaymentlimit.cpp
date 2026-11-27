#include "domain/common/rules/cashpaymentlimit.h"

#include <QLocale>

#include <cmath>

namespace domain {

namespace {

int boundedMultiple(int multiple)
{
    return multiple >= 1 && multiple <= CashLimitTerms::kLegalUmaMultiple
               ? multiple
               : CashLimitTerms::kLegalUmaMultiple;
}

} // namespace

CashPaymentLimit::CashPaymentLimit(const CashLimitTerms &terms)
    : m_umaDailyValue(terms.umaDailyValue)
    , m_umaMultiple(boundedMultiple(terms.umaMultiple))
{
}

CashPaymentLimit::CashPaymentLimit(double umaDailyValue)
    : CashPaymentLimit(CashLimitTerms{umaDailyValue, CashLimitTerms::kLegalUmaMultiple})
{
}

bool CashPaymentLimit::isConfigured() const
{
    return m_umaDailyValue > 0.0;
}

int CashPaymentLimit::umaMultiple() const
{
    return m_umaMultiple;
}

qint64 CashPaymentLimit::limitCents() const
{
    // La UMA trae centavos (117.31): se redondea a centavos y se multiplica
    // en enteros. En double, 3210 × 117.31 da 376565.10000000003 y el importe
    // 376,565.10 se guarda como 376565.09999999998, así que la comparación
    // directa dejaba pasar un pago exactamente en el tope.
    return isConfigured() ? std::llround(m_umaDailyValue * 100.0) * m_umaMultiple : 0;
}

double CashPaymentLimit::limit() const
{
    return static_cast<double>(limitCents()) / 100.0;
}

bool CashPaymentLimit::allows(PaymentMethod method, double amount) const
{
    if (method != PaymentMethod::Efectivo)
        return true;
    if (!isConfigured())
        return false;
    // La ley dice "igual o superior": alcanzar el tope ya está prohibido.
    return std::llround(amount * 100.0) < limitCents();
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
    const QLocale mexico(QLocale::Spanish, QLocale::Mexico);
    result.addError(field,
                    QStringLiteral("El pago en efectivo no puede alcanzar %1 UMA ($%2). "
                                   "Cambia el método de pago o ajusta el precio.")
                        .arg(mexico.toString(m_umaMultiple), mexico.toString(limit(), 'f', 2)));
}

} // namespace domain
