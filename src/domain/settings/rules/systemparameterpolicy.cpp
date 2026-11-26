#include "domain/settings/rules/systemparameterpolicy.h"

#include "domain/common/value_objects/cashlimitterms.h"

#include <QLocale>

namespace domain {

void SystemParameterPolicy::check(SystemParameter parameter, qint64 hundredths,
                                  ValidationResult &result)
{
    const QString field = fieldName(parameter);
    if (hundredths > kMaxHundredths) {
        result.addError(field, QStringLiteral("El valor no puede pasar de 9,999,999,999.99."));
        return;
    }

    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        if (hundredths <= 0) {
            result.addError(field, QStringLiteral("La UMA debe ser mayor que cero."));
        } else if (hundredths > kMaxUmaHundredths) {
            result.addError(field, QStringLiteral("La UMA diaria no puede pasar de $1,000.00: revisa "
                                                  "el punto decimal."));
        }
        break;
    case SystemParameter::CashLimitUmaMultiple: {
        const qint64 legal = qint64(CashLimitTerms::kLegalUmaMultiple) * 100;
        if (hundredths < 100 || hundredths > legal || hundredths % 100 != 0) {
            result.addError(field,
                            QStringLiteral("El tope debe ser un número entero de UMA, de 1 a "
                                           "%1: la ley no permite uno mayor.")
                                .arg(QLocale(QLocale::Spanish, QLocale::Mexico)
                                         .toString(CashLimitTerms::kLegalUmaMultiple)));
        }
        break;
    }
    }
}

} // namespace domain
