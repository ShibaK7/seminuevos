#include "application/auth/services/authenticationservice.h"

namespace application {

namespace {
// El mismo mensaje para "no existe" y "contraseña equivocada": distinguirlos
// le diría a un tercero qué usuarios existen.
QString wrongCredentialsMessage()
{
    return QStringLiteral("Usuario o contraseña incorrectos. Verifica tus datos.");
}
} // namespace

AuthenticationService::AuthenticationService(UserDirectory &users, const PasswordHasher &hasher)
    : m_users(users)
    , m_hasher(hasher)
{
}

LoginResult AuthenticationService::login(const QString &username, const QString &password) const
{
    LoginResult result;

    const QString user = username.trimmed();
    if (user.isEmpty() || password.isEmpty()) {
        result.errorMessage = QStringLiteral("Captura el usuario y la contraseña.");
        return result;
    }

    QString lookupError;
    const std::optional<UserRecordDto> record = m_users.findByUsername(user, &lookupError);
    if (!record) {
        result.errorMessage = lookupError.isEmpty()
                                  ? wrongCredentialsMessage()
                                  : QStringLiteral("No se pudo consultar el usuario: %1").arg(lookupError);
        return result;
    }

    if (!m_hasher.verify(password, record->passwordHash)) {
        result.errorMessage = wrongCredentialsMessage();
        return result;
    }

<<<<<<< HEAD
    QString loginError;
    if (!m_users.recordSuccessfulLogin(record->id, &loginError)) {
        result.errorMessage = QStringLiteral("No se pudo actualizar la última sesión: %1").arg(loginError);
=======
    // Un rol que la aplicación no conoce no entra: adivinarle permisos sería
    // peor que decir qué pasa. El CHECK de users.role solo admite los dos
    // que existen, así que esto solo ocurre con datos editados a mano.
    const std::optional<domain::UserRole> role = domain::userRoleFromDb(record->role);
    if (!role) {
        result.errorMessage =
            QStringLiteral("La cuenta tiene un rol que la aplicación no reconoce (%1).").arg(record->role);
>>>>>>> 1041035 (Carry the session role as a domain enum)
        return result;
    }

    result.ok = true;
<<<<<<< HEAD
    result.session = SessionDto{user, record->displayName, record->role, record->id};
=======
    result.session = SessionDto{user, record->displayName, *role};
>>>>>>> 1041035 (Carry the session role as a domain enum)
    return result;
}

} // namespace application
