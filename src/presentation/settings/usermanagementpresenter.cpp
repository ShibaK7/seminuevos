#include "presentation/settings/usermanagementpresenter.h"

#include "application/settings/services/useradministrationservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/settings/iusermanagementview.h"

namespace presentation {

UserManagementPresenter::UserManagementPresenter(
    IUserManagementView &view, const application::UserAdministrationService &service,
    TaskRunner &runner, const application::SessionDto &actor, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
    , m_actor(actor)
{
}

void UserManagementPresenter::start()
{
    load();
}

void UserManagementPresenter::refresh()
{
    if (!m_saving)
        load();
}

void UserManagementPresenter::load(bool keepMessage)
{
    const int generation = ++m_loadGeneration;
    m_view.setBusy(true);
    const application::UserAdministrationService *service = &m_service;
    const application::SessionDto actor = m_actor;
    m_runner.run(
        this, [service, actor] { return service->listUsers(actor); },
        [this, generation, keepMessage](const application::UserAccountListResult &result) {
            using Status = application::UserAccountListResult::Status;
            if (generation != m_loadGeneration)
                return;
            m_view.setBusy(m_saving);
            switch (result.status) {
            case Status::Loaded:
                m_users = result.users;
                m_view.showUsers(m_users);
                if (!keepMessage)
                    m_view.showMessage(QString());
                break;
            case Status::Denied:
                m_view.showMessage(QStringLiteral("Tu rol no puede administrar usuarios."));
                break;
            case Status::Failed:
                m_view.showMessage(QStringLiteral("No se pudo leer la lista de usuarios: %1")
                                       .arg(result.errorMessage));
                break;
            }
        });
}

std::optional<application::UserAccountDto> UserManagementPresenter::selectedUser()
{
    const int id = m_view.selectedUserId();
    for (const application::UserAccountDto &user : std::as_const(m_users)) {
        if (user.id == id)
            return user;
    }
    m_view.showMessage(QStringLiteral("Selecciona una cuenta en la tabla."));
    return std::nullopt;
}

void UserManagementPresenter::onAddUser()
{
    if (m_saving)
        return;
    m_view.showMessage(QString());
    m_view.askNewUser();
}

void UserManagementPresenter::onEditUser()
{
    if (m_saving)
        return;
    if (const auto user = selectedUser()) {
        m_view.showMessage(QString());
        m_view.askUserEdit(*user);
    }
}

void UserManagementPresenter::onResetPassword()
{
    if (m_saving)
        return;
    if (const auto user = selectedUser()) {
        m_view.showMessage(QString());
        m_view.askPasswordReset(*user);
    }
}

void UserManagementPresenter::onToggleActive()
{
    if (m_saving)
        return;
    const auto user = selectedUser();
    if (!user)
        return;
    if (!user->canChangeActive) {
        m_view.showMessage(QStringLiteral("No puedes desactivar tu propia cuenta."));
        return;
    }
    const bool activate = !user->active;
    if (!m_view.confirmActivation(*user, activate))
        return;

    const application::UserAdministrationService *service = &m_service;
    const application::SessionDto actor = m_actor;
    const int id = user->id;
    const QString version = user->version;
    write([service, actor, id, version, activate] {
              return service->setActive(id, version, activate, actor);
          },
          activate ? QStringLiteral("Se reactivó la cuenta de %1.").arg(user->displayName)
                   : QStringLiteral("Se desactivó la cuenta de %1: ya no puede entrar.")
                         .arg(user->displayName),
          false);
}

void UserManagementPresenter::onSubmitNewUser(const application::NewUserAccountDto &user)
{
    const application::UserAdministrationService *service = &m_service;
    const application::SessionDto actor = m_actor;
    write([service, actor, user] { return service->createUser(user, actor); },
          QStringLiteral("Se dio de alta la cuenta de %1.").arg(user.displayName.trimmed()), true);
}

void UserManagementPresenter::onSubmitUserEdit(const application::UserAccountUpdateDto &user)
{
    const application::UserAdministrationService *service = &m_service;
    const application::SessionDto actor = m_actor;
    write([service, actor, user] { return service->updateUser(user, actor); },
          QStringLiteral("Se guardaron los cambios de %1.").arg(user.displayName.trimmed()), true);
}

void UserManagementPresenter::onSubmitPasswordReset(const application::PasswordResetDto &reset)
{
    const application::UserAdministrationService *service = &m_service;
    const application::SessionDto actor = m_actor;
    write([service, actor, reset] { return service->resetPassword(reset, actor); },
          QStringLiteral("Se cambió la contraseña."), true);
}

void UserManagementPresenter::write(std::function<application::UserAccountWriteResult()> work,
                                    const QString &done, bool fromForm)
{
    if (m_saving)
        return;
    m_saving = true;
    m_view.setBusy(true);
    if (fromForm)
        m_view.setUserFormBusy(true);
    m_view.showMessage(QString());
    m_view.showNotice(QString());
    m_runner.run(this, std::move(work),
                 [this, done, fromForm](const application::UserAccountWriteResult &result) {
                     using Status = application::UserAccountWriteResult::Status;
                     m_saving = false;
                     m_view.setBusy(false);
                     if (fromForm)
                         m_view.setUserFormBusy(false);
                     switch (result.status) {
                     case Status::Saved:
                         if (fromForm)
                             m_view.closeUserForm();
                         m_view.showNotice(done);
                         load();
                         return;
                     case Status::Rejected:
                         if (fromForm)
                             m_view.showUserFormErrors(result.validation.errors());
                         else
                             m_view.showMessage(result.validation.joinedMessages());
                         return;
                     case Status::Conflict:
                         if (fromForm)
                             m_view.closeUserForm();
                         m_view.showMessage(QStringLiteral("Otro administrador cambió esta cuenta "
                                                           "mientras tanto; se recargó la lista. "
                                                           "Revísala y vuelve a intentarlo."));
                         load(true);
                         return;
                     case Status::NotFound:
                         if (fromForm)
                             m_view.closeUserForm();
                         m_view.showMessage(QStringLiteral("La cuenta ya no existe."));
                         load(true);
                         return;
                     case Status::Denied:
                         if (fromForm)
                             m_view.closeUserForm();
                         m_view.showMessage(QStringLiteral("Tu cuenta ya no puede administrar "
                                                           "usuarios."));
                         return;
                     case Status::Failed:
                         // El formulario sigue abierto con lo capturado.
                         m_view.showMessage(QStringLiteral("No se guardó: %1").arg(result.errorMessage));
                         return;
                     }
                 });
}

} // namespace presentation
