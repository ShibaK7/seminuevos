#include "domain/settings/rules/systemparameterpolicy.h"

#include "domain/common/value_objects/cashlimitterms.h"

#include <QLocale>

namespace domain {

namespace {

QString pesos(qint64 hundredths)
{
    return QLocale(QLocale::Spanish, QLocale::Mexico).toString(double(hundredths) / 100.0, 'f', 2);
}

} // namespace

void SystemParameterPolicy::check(SystemParameter parameter, qint64 hundredths,
                                  std::optional<qint64> current, ValidationResult &result)
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
        } else if (current && *current > 0
                   && (hundredths > *current * kMaxUmaJump || hundredths * kMaxUmaJump < *current)) {
            result.addError(field, QStringLiteral("La UMA nueva no puede ser más del doble ni menos de "
                                                  "la mitad de la vigente ($%1): revisa el punto "
                                                  "decimal.")
                                       .arg(pesos(*current)));
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
