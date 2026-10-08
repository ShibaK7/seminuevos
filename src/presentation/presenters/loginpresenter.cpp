#include "presentation/presenters/loginpresenter.h"

#include "application/services/authenticationservice.h"
#include "presentation/presenters/iloginview.h"
#include "presentation/tasks/taskrunner.h"

namespace presentation {

LoginPresenter::LoginPresenter(ILoginView &view, const application::AuthenticationService &auth,
                               TaskRunner &runner, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_auth(auth)
    , m_runner(runner)
{
}

void LoginPresenter::login(const QString &username, const QString &password)
{
    // Un segundo Enter mientras se verifica no lanza otra consulta.
    if (m_busy)
        return;
    m_busy = true;
    m_view.setBusy(true);

    const application::AuthenticationService *auth = &m_auth;
    m_runner.run(
        this,
        [auth, username, password] { return auth->login(username, password); },
        [this](const application::LoginResult &result) {
            m_busy = false;
            m_view.setBusy(false);
            if (result.ok)
                emit authenticated(result.session);
            else
                m_view.showLoginError(result.errorMessage);
        });
}

} // namespace presentation
