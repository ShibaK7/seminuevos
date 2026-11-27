#ifndef APPLICATION_SETTINGS_PORTS_SETTINGSREPOSITORY_H
#define APPLICATION_SETTINGS_PORTS_SETTINGSREPOSITORY_H

#include "application/settings/dto/settingsdtos.h"

#include <QList>
#include <QString>

#include <optional>

namespace application {

// Puerto: los parámetros globales del negocio (global_configurations).
class SettingsRepository
{
public:
    // Un parámetro que cambió: su valor nuevo y el que tenía al leerse.
    struct Change
    {
        domain::SystemParameter parameter = domain::SystemParameter::UmaDailyValue;
        // nullopt si al leerse no existía su renglón.
        std::optional<qint64> expected;
        qint64 value = 0;
    };

    struct UpdateOutcome
    {
        enum class Status {
            Updated,  // se guardaron todos
            Conflict, // alguno ya no tenía el valor esperado; no se guardó ninguno
            ActorNotAllowed, // quien guarda ya no es Administrador activo; no se guardó ninguno
            Failed,   // falló la base; no se guardó ninguno
        };
        Status status = Status::Failed;
        QString errorMessage; // en Failed
    };

    virtual ~SettingsRepository() = default;

    // #83: los parámetros que tienen renglón, con la fecha de su último
    // cambio. Si la consulta falla devuelve vacío y deja la causa en `error`.
    virtual QList<SystemParameterDto> getSystemParameters(QString *error) = 0;

    // #84: guarda los cambios en una sola transacción, todo o nada, y solo
    // ellos reciben fecha nueva. Un parámetro que ya no tiene el valor
    // esperado es un conflicto: otro administrador lo cambió. Dentro de la
    // misma transacción comprueba que `actorId` siga siendo Administrador
    // activo: la sesión pudo abrirse antes de que le quitaran el rol.
    virtual UpdateOutcome updateSystemParameters(const QList<Change> &changes, int actorId) = 0;

protected:
    SettingsRepository() = default;
    SettingsRepository(const SettingsRepository &) = default;
    SettingsRepository &operator=(const SettingsRepository &) = default;
};

} // namespace application

#endif // APPLICATION_SETTINGS_PORTS_SETTINGSREPOSITORY_H
