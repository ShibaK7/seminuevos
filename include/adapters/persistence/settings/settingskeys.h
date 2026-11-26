#ifndef ADAPTERS_PERSISTENCE_SETTINGS_SETTINGSKEYS_H
#define ADAPTERS_PERSISTENCE_SETTINGS_SETTINGSKEYS_H

#include <QString>

// Las llaves de global_configurations. Van en un solo lugar porque las leen
// varios adaptadores (los catálogos del alta, Configuraciones) y una llave
// mal escrita no falla: solo deja el parámetro "sin configurar".
namespace settingskeys {

inline QString umaDailyValue() { return QStringLiteral("UMA_DIARIA"); }
inline QString cashLimitUmaMultiple() { return QStringLiteral("TOPE_EFECTIVO_UMA"); }

} // namespace settingskeys

#endif // ADAPTERS_PERSISTENCE_SETTINGS_SETTINGSKEYS_H
