#include "domain/settings/rules/useraccountpolicy.h"

#include <QRegularExpression>

namespace domain {

QString UserAccountPolicy::normalizeUsername(const QString &username)
{
    return username.trimmed().toLower();
}

void UserAccountPolicy::checkUsername(const QString &username, ValidationResult &result)
{
    static const QRegularExpression format(QStringLiteral("^[a-z0-9][a-z0-9._-]{2,49}$"));
    if (username.isEmpty()) {
        result.addError(QStringLiteral("username"), QStringLiteral("Captura el usuario."));
    } else if (!format.match(username).hasMatch()) {
        result.addError(QStringLiteral("username"),
                        QStringLiteral("El usuario lleva de 3 a 50 caracteres: letras sin acento, "
                                       "números, punto, guion o guion bajo, y empieza con letra o "
                                       "número."));
    }
}

void UserAccountPolicy::checkDisplayName(const QString &displayName, ValidationResult &result)
{
    const QString name = displayName.trimmed();
    if (name.isEmpty())
        result.addError(QStringLiteral("displayName"), QStringLiteral("Captura el nombre."));
    else if (name.size() > 100)
        result.addError(QStringLiteral("displayName"),
                        QStringLiteral("El nombre admite hasta 100 caracteres."));
}

void UserAccountPolicy::checkPassword(const QString &password, const QString &confirmation,
                                      ValidationResult &result)
{
    if (password.size() < kMinPasswordLength || password.size() > kMaxPasswordLength) {
        result.addError(QStringLiteral("password"),
                        QStringLiteral("La contraseña lleva de %1 a %2 caracteres.")
                            .arg(kMinPasswordLength)
                            .arg(kMaxPasswordLength));
        return;
    }
    if (confirmation != password) {
        result.addError(QStringLiteral("passwordConfirmation"),
                        QStringLiteral("Las contraseñas no coinciden."));
    }
}

bool UserAccountPolicy::canChangeRole(int actorId, int targetId)
{
    return actorId != targetId;
}

bool UserAccountPolicy::canChangeActive(int actorId, int targetId)
{
    return actorId != targetId;
}

void UserAccountPolicy::checkRoleChange(int actorId, int targetId, UserRole currentRole,
                                        UserRole requestedRole, ValidationResult &result)
{
    if (requestedRole != currentRole && !canChangeRole(actorId, targetId))
        result.addError(QStringLiteral("role"), QStringLiteral("No puedes cambiar tu propio rol."));
}

void UserAccountPolicy::checkActivation(int actorId, int targetId, ValidationResult &result)
{
    if (!canChangeActive(actorId, targetId)) {
        result.addError(QStringLiteral("active"),
                        QStringLiteral("No puedes desactivar tu propia cuenta."));
    }
}

} // namespace domain
