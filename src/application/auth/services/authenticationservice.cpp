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

    result.ok = true;
    result.session = SessionDto{user, record->displayName, record->role};
    return result;
}

} // namespace application
