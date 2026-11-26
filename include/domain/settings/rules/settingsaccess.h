#ifndef DOMAIN_SETTINGS_RULES_SETTINGSACCESS_H
#define DOMAIN_SETTINGS_RULES_SETTINGSACCESS_H

#include "domain/common/value_objects/enums.h"

namespace domain {

// Quién entra a Configuraciones (US-07.1). Es el único lugar que lo decide:
// el menú, la pantalla y los servicios le preguntan a esta clase.
//
// Las preguntas hoy tienen la misma respuesta ("¿es Administrador?"), pero son
// decisiones distintas del negocio y cada una puede cambiar por su lado.
class SettingsAccess
{
public:
    explicit SettingsAccess(UserRole role);

    // UMA, tope de efectivo y saldos.
    bool canManageSystemParameters() const;
    // Alta, edición y desactivación de usuarios.
    bool canManageUsers() const;
    // Si el menú muestra Configuraciones.
    bool canOpenSettings() const;

private:
    UserRole m_role;
};

} // namespace domain

#endif // DOMAIN_SETTINGS_RULES_SETTINGSACCESS_H
