#include "domain/settings/value_objects/systemparameter.h"

#include "domain/common/value_objects/cashlimitterms.h"

namespace domain {

QList<SystemParameter> allSystemParameters()
{
    return {SystemParameter::UmaDailyValue, SystemParameter::CashLimitUmaMultiple};
}

QString displayLabel(SystemParameter parameter)
{
    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        return QStringLiteral("Valor 1 UMA");
    case SystemParameter::CashLimitUmaMultiple:
        return QStringLiteral("Máximo valor pagos en efectivo");
    }
    return QString();
}

ParameterUnit unitOf(SystemParameter parameter)
{
    return parameter == SystemParameter::CashLimitUmaMultiple ? ParameterUnit::Uma
                                                              : ParameterUnit::Pesos;
}

std::optional<qint64> defaultHundredths(SystemParameter parameter)
{
    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        return std::nullopt;
    case SystemParameter::CashLimitUmaMultiple:
        return qint64(CashLimitTerms::kLegalUmaMultiple) * 100;
    }
    return std::nullopt;
}

QString fieldName(SystemParameter parameter)
{
    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        return QStringLiteral("umaDailyValue");
    case SystemParameter::CashLimitUmaMultiple:
        return QStringLiteral("cashLimitUmaMultiple");
    }
    return QString();
}

} // namespace domain
