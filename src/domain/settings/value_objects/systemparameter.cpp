#include "domain/settings/value_objects/systemparameter.h"

#include "domain/common/value_objects/cashlimitterms.h"

namespace domain {

QList<SystemParameter> allSystemParameters()
{
    return {SystemParameter::UmaDailyValue, SystemParameter::CashLimitUmaMultiple,
            SystemParameter::BankBalance, SystemParameter::PettyCashBalance,
            SystemParameter::SafeBalance};
}

QString displayLabel(SystemParameter parameter)
{
    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        return QStringLiteral("Valor 1 UMA");
    case SystemParameter::CashLimitUmaMultiple:
        return QStringLiteral("Máximo valor pagos en efectivo");
    case SystemParameter::BankBalance:
        return QStringLiteral("Cantidad Banco");
    case SystemParameter::PettyCashBalance:
        return QStringLiteral("Caja Chica");
    case SystemParameter::SafeBalance:
        return QStringLiteral("Caja Fuerte");
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
    case SystemParameter::BankBalance:
    case SystemParameter::PettyCashBalance:
    case SystemParameter::SafeBalance:
        return 0;
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
    case SystemParameter::BankBalance:
        return QStringLiteral("bankBalance");
    case SystemParameter::PettyCashBalance:
        return QStringLiteral("pettyCashBalance");
    case SystemParameter::SafeBalance:
        return QStringLiteral("safeBalance");
    }
    return QString();
}

} // namespace domain
