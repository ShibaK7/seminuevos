#ifndef DOMAIN_SETTINGS_RULES_USERACCOUNTPOLICY_H
#define DOMAIN_SETTINGS_RULES_USERACCOUNTPOLICY_H

#include "domain/common/value_objects/enums.h"
#include "domain/common/value_objects/validationresult.h"

#include <QString>

namespace domain {

// Las reglas de las cuentas que el Administrador da de alta y corrige en
// Configuraciones (US-07.1):
//   - el usuario se guarda en minúsculas, de 3 a 50 caracteres: letras sin
//     acento, números, punto, guion y guion bajo, empezando con letra o
//     número. Así "Ana.Lopez" y "ana.lopez" no pueden ser dos cuentas;
//   - el nombre para mostrar es obligatorio, hasta 100 caracteres;
//   - la contraseña lleva de 8 a 128 caracteres y se captura dos veces;
//   - nadie se cambia su propio rol ni desactiva su propia cuenta: un
//     Administrador que se quitara el rol por error ya no podría
//     deshacerlo. Que siempre quede uno activo lo cuida además la base.
class UserAccountPolicy
{
public:
    static constexpr int kMinPasswordLength = 8;
    static constexpr int kMaxPasswordLength = 128;

    static QString normalizeUsername(const QString &username);

    // Campo "username". Recibe el usuario ya normalizado.
    static void checkUsername(const QString &username, ValidationResult &result);
    // Campo "displayName".
    static void checkDisplayName(const QString &displayName, ValidationResult &result);
    // Campos "password" y "passwordConfirmation". No se recortan espacios:
    // también cuentan.
    static void checkPassword(const QString &password, const QString &confirmation,
                              ValidationResult &result);

    // Si quien actúa (actorId) puede cambiarle el rol, o activarla y
    // desactivarla, a la cuenta targetId.
    static bool canChangeRole(int actorId, int targetId);
    static bool canChangeActive(int actorId, int targetId);

    // Campo "role".
    static void checkRoleChange(int actorId, int targetId, UserRole currentRole,
                                UserRole requestedRole, ValidationResult &result);
    // Campo "active".
    static void checkActivation(int actorId, int targetId, ValidationResult &result);
};

} // namespace domain

#endif // DOMAIN_SETTINGS_RULES_USERACCOUNTPOLICY_H
