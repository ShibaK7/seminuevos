#ifndef APPLICATION_SETTINGS_SERVICES_SYSTEMSETTINGSSERVICE_H
#define APPLICATION_SETTINGS_SERVICES_SYSTEMSETTINGSSERVICE_H

#include "application/auth/dto/authdtos.h"
#include "application/settings/dto/settingsdtos.h"
#include "application/settings/ports/settingsrepository.h"
#include "domain/common/value_objects/enums.h"

namespace application {

// Caso de uso "consultar y corregir los parámetros del negocio" (US-07.1, #83
// y #84). Cada operación exige primero que el rol entre a Configuraciones
// (SettingsAccess): ocultar el menú no basta. Al guardar, el repositorio
// vuelve a comprobar en la base que quien actúa siga siendo Administrador
// activo.
//
// Solo guarda una referencia a su puerto, así que se puede llamar desde el
// hilo de trabajo. Hace E/S: correrlo fuera del hilo de la interfaz.
class SystemSettingsService final
{
public:
    explicit SystemSettingsService(SettingsRepository &settings);

    // Los cinco parámetros, en el orden de pantalla. El que nunca se capturó
    // viene sin valor ni fecha.
    SystemParametersResult getSystemParameters(const SessionDto &actor) const;

    // Guarda los que cambiaron respecto a lo que se leyó (o, si nunca se
    // capturaron, respecto a su valor por omisión). Valida solo esos: un
    // parámetro que nadie tocó no se rechaza por una regla nueva. Si nada
    // cambió, no toca la base.
    SystemParametersUpdateResult updateSystemParameters(const QList<SystemParameterEditDto> &edits,
                                                        const SessionDto &actor) const;

private:
    SettingsRepository &m_settings;
};

} // namespace application

#endif // APPLICATION_SETTINGS_SERVICES_SYSTEMSETTINGSSERVICE_H
