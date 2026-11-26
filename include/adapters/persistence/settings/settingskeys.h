#ifndef ADAPTERS_PERSISTENCE_SETTINGS_SETTINGSKEYS_H
#define ADAPTERS_PERSISTENCE_SETTINGS_SETTINGSKEYS_H

#include "domain/settings/value_objects/systemparameter.h"

#include <QString>

#include <optional>

// Las llaves de global_configurations. Van en un solo lugar porque las leen
// varios adaptadores (los catálogos del alta, Configuraciones) y una llave
// mal escrita no falla: solo deja el parámetro "sin configurar".
namespace settingskeys {

inline QString umaDailyValue() { return QStringLiteral("UMA_DIARIA"); }
inline QString cashLimitUmaMultiple() { return QStringLiteral("TOPE_EFECTIVO_UMA"); }
inline QString bankBalance() { return QStringLiteral("SALDO_BANCO"); }
inline QString pettyCashBalance() { return QStringLiteral("SALDO_CAJA_CHICA"); }
inline QString safeBalance() { return QStringLiteral("SALDO_CAJA_FUERTE"); }

inline QString keyOf(domain::SystemParameter parameter)
{
    switch (parameter) {
    case domain::SystemParameter::UmaDailyValue:
        return umaDailyValue();
    case domain::SystemParameter::CashLimitUmaMultiple:
        return cashLimitUmaMultiple();
    case domain::SystemParameter::BankBalance:
        return bankBalance();
    case domain::SystemParameter::PettyCashBalance:
        return pettyCashBalance();
    case domain::SystemParameter::SafeBalance:
        return safeBalance();
    }
    return QString();
}

inline std::optional<domain::SystemParameter> parameterOf(const QString &key)
{
    for (const domain::SystemParameter parameter : domain::allSystemParameters()) {
        if (keyOf(parameter) == key)
            return parameter;
    }
    return std::nullopt;
}

} // namespace settingskeys

#endif // ADAPTERS_PERSISTENCE_SETTINGS_SETTINGSKEYS_H
