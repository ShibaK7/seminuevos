#include "application/settings/services/useradministrationservice.h"

#include "domain/settings/rules/settingsaccess.h"
#include "domain/settings/rules/useraccountpolicy.h"

namespace application {

namespace {

using Policy = domain::UserAccountPolicy;
using Status = UserAccountWriteResult::Status;
using Outcome = UserAccountRepository::Outcome;

bool canManage(const SessionDto &actor)
{
    return domain::SettingsAccess(actor.role).canManageUsers();
}

UserAccountWriteResult withStatus(Status status)
{
    UserAccountWriteResult result;
    result.status = status;
    return result;
}

UserAccountWriteResult rejected(const domain::ValidationResult &validation)
{
    UserAccountWriteResult result = withStatus(Status::Rejected);
    result.validation = validation;
    return result;
}

UserAccountWriteResult rejected(const QString &field, const QString &message)
{
    domain::ValidationResult validation;
    validation.addError(field, message);
    return rejected(validation);
}

// Lo que dijo la base, en términos de la pantalla. `adminField` es el campo
// donde se marca "debe quedar un Administrador activo".
UserAccountWriteResult fromOutcome(const Outcome &outcome, const QString &adminField)
{
    switch (outcome.status) {
    case Outcome::Status::Ok: {
        UserAccountWriteResult result = withStatus(Status::Saved);
        result.id = outcome.id;
        return result;
    }
    case Outcome::Status::DuplicateUsername:
        return rejected(QStringLiteral("username"), QStringLiteral("Ya hay una cuenta con ese usuario."));
    case Outcome::Status::LastActiveAdmin:
        return rejected(adminField, QStringLiteral("Debe quedar al menos un Administrador activo."));
    case Outcome::Status::NotFound:
        return withStatus(Status::NotFound);
    case Outcome::Status::Conflict:
        return withStatus(Status::Conflict);
    case Outcome::Status::ActorNotAllowed:
        return withStatus(Status::Denied);
    case Outcome::Status::Failed:
        break;
    }
    UserAccountWriteResult result = withStatus(Status::Failed);
    result.errorMessage = outcome.errorMessage;
    return result;
}

} // namespace

UserAdministrationService::UserAdministrationService(UserAccountRepository &accounts,
                                                     const PasswordHasher &hasher)
    : m_accounts(accounts)
    , m_hasher(hasher)
{
}

UserAccountListResult UserAdministrationService::listUsers(const SessionDto &actor) const
{
    using ListStatus = UserAccountListResult::Status;
    UserAccountListResult result;
    if (!canManage(actor)) {
        result.status = ListStatus::Denied;
        return result;
    }

    QString error;
    result.users = m_accounts.listUsers(&error);
    if (!error.isEmpty()) {
        result.status = ListStatus::Failed;
        result.users.clear();
        result.errorMessage = error;
        return result;
    }
    for (UserAccountDto &user : result.users) {
        user.isSelf = user.id == actor.userId;
        user.canChangeRole = Policy::canChangeRole(actor.userId, user.id);
        user.canChangeActive = Policy::canChangeActive(actor.userId, user.id);
    }
    result.status = ListStatus::Loaded;
    return result;
}

UserAccountWriteResult UserAdministrationService::createUser(const NewUserAccountDto &user,
                                                             const SessionDto &actor) const
{
    if (!canManage(actor))
        return withStatus(Status::Denied);

    const QString username = Policy::normalizeUsername(user.username);
    domain::ValidationResult validation;
    Policy::checkUsername(username, validation);
    Policy::checkDisplayName(user.displayName, validation);
    Policy::checkPassword(user.password, user.passwordConfirmation, validation);
    if (!validation.isValid())
        return rejected(validation);

    const UserAccountRepository::NewUserRecord record{username, user.displayName.trimmed(),
                                                      user.role, m_hasher.hash(user.password)};
    return fromOutcome(m_accounts.createUser(record, actor.userId), QStringLiteral("role"));
}

UserAccountWriteResult UserAdministrationService::updateUser(const UserAccountUpdateDto &user,
                                                             const SessionDto &actor) const
{
    if (!canManage(actor))
        return withStatus(Status::Denied);

    domain::ValidationResult validation;
    Policy::checkDisplayName(user.displayName, validation);
    // Si es la propia cuenta, su rol actual es el de la sesión.
    Policy::checkRoleChange(actor.userId, user.id, actor.role, user.role, validation);
    if (!validation.isValid())
        return rejected(validation);

    UserAccountUpdateDto trimmed = user;
    trimmed.displayName = user.displayName.trimmed();
    return fromOutcome(m_accounts.updateUser(trimmed, actor.userId), QStringLiteral("role"));
}

UserAccountWriteResult UserAdministrationService::setActive(int userId, const QString &version,
                                                            bool active,
                                                            const SessionDto &actor) const
{
    if (!canManage(actor))
        return withStatus(Status::Denied);

    domain::ValidationResult validation;
    Policy::checkActivation(actor.userId, userId, validation);
    if (!validation.isValid())
        return rejected(validation);

    return fromOutcome(m_accounts.setActive(userId, version, active, actor.userId),
                       QStringLiteral("active"));
}

UserAccountWriteResult UserAdministrationService::resetPassword(const PasswordResetDto &reset,
                                                                const SessionDto &actor) const
{
    if (!canManage(actor))
        return withStatus(Status::Denied);

    domain::ValidationResult validation;
    Policy::checkPassword(reset.password, reset.passwordConfirmation, validation);
    if (!validation.isValid())
        return rejected(validation);

    return fromOutcome(m_accounts.resetPassword(reset.id, m_hasher.hash(reset.password), actor.userId),
                       QStringLiteral("password"));
}

} // namespace application
