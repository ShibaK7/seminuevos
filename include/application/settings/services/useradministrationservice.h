#ifndef APPLICATION_SETTINGS_SERVICES_USERADMINISTRATIONSERVICE_H
#define APPLICATION_SETTINGS_SERVICES_USERADMINISTRATIONSERVICE_H

#include "application/auth/dto/authdtos.h"
#include "application/auth/ports/passwordhasher.h"
#include "application/settings/dto/useraccountdtos.h"
#include "application/settings/ports/useraccountrepository.h"

namespace application {

// Caso de uso "administrar las cuentas de usuario" desde Configuraciones
// (US-07.1): alta, nombre y rol, contraseña, y activar o desactivar. Cada
// operación recibe la sesión de quien la hace y exige primero que su rol
// administre usuarios (SettingsAccess); después las reglas del dominio, y al
// final la base, que vuelve a comprobar quién actúa y que quede un
// Administrador activo.
//
// Solo guarda referencias a sus puertos, así que se puede llamar desde el
// hilo de trabajo. Hace E/S, y el hash de la contraseña es lento a propósito
// (PBKDF2): correrlo fuera del hilo de la interfaz.
class UserAdministrationService final
{
public:
    UserAdministrationService(UserAccountRepository &accounts, const PasswordHasher &hasher);

    UserAccountListResult listUsers(const SessionDto &actor) const;
    UserAccountWriteResult createUser(const NewUserAccountDto &user, const SessionDto &actor) const;
    UserAccountWriteResult updateUser(const UserAccountUpdateDto &user, const SessionDto &actor) const;
    UserAccountWriteResult setActive(int userId, const QString &version, bool active,
                                     const SessionDto &actor) const;
    UserAccountWriteResult resetPassword(const PasswordResetDto &reset,
                                         const SessionDto &actor) const;

private:
    UserAccountRepository &m_accounts;
    const PasswordHasher &m_hasher;
};

} // namespace application

#endif // APPLICATION_SETTINGS_SERVICES_USERADMINISTRATIONSERVICE_H
