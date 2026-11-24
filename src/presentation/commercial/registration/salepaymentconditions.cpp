#include "salepaymentconditions.h"

#include "adapters/contract/numbertowordses.h"

#include <QLocale>

namespace presentation {
namespace {

QString percentText(double percent)
{
    QString text = QString::number(percent, 'f', 2);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

} // namespace

QString salePaymentConditions(const application::SaleRegistrationDto &sale)
{
    if (sale.paymentType == domain::PaymentType::Contado) {
        const QString method = sale.paymentMethod
                                  ? domain::toDbString(*sale.paymentMethod).toUpper()
                                  : QStringLiteral("MÉTODO NO DEFINIDO");
        return QStringLiteral("PAGO DE CONTADO (%1) EN UNA SOLA EXHIBICIÓN.").arg(method);
    }

    if (!sale.financing)
        return QStringLiteral("CONDICIONES DE CRÉDITO NO DISPONIBLES.");

    const auto locale = QLocale(QLocale::Spanish, QLocale::Mexico);
    const double downPayment = sale.financing->downPayment;
    return QStringLiteral(
               "ENGANCHE POR %1 (%2) Y EL RESTO A %3 MESES CON UNA TASA DE INTERÉS DEL "
               "%4% SOBRE SALDOS INSOLUTOS.")
        .arg(locale.toCurrencyString(downPayment, QStringLiteral("$"), 2),
             NumberToWordsEs::convert(downPayment),
             QString::number(sale.financing->monthsTerm),
             percentText(sale.financing->interestRatePercent));
}

} // namespace presentation
