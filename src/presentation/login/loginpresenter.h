#ifndef PRESENTATION_LOGIN_LOGINPRESENTER_H
#define PRESENTATION_LOGIN_LOGINPRESENTER_H

#include "application/auth/dto/authdtos.h"

#include <QObject>

namespace application {
class AuthenticationService;
}

namespace presentation {

class ILoginView;
class TaskRunner;

// Presenter del inicio de sesión: recibe el intento de la vista, corre el caso
// de uso fuera del hilo de la interfaz y le dice a la vista qué mostrar. No
// sabe de widgets (solo QtCore), así que se prueba con una vista falsa.
//
// Es QObject solo para dos cosas: servir de contexto al TaskRunner (si se
// destruye, el resultado ya no se entrega) y avisar con authenticated() a la
// raíz de composición, que es quien abre la ventana principal.
class LoginPresenter final : public QObject
{
    Q_OBJECT

public:
    LoginPresenter(ILoginView &view, const application::AuthenticationService &auth,
                   TaskRunner &runner, QObject *parent = nullptr);

    void login(const QString &username, const QString &password);

signals:
    void authenticated(const application::SessionDto &session);

private:
    ILoginView &m_view;
    const application::AuthenticationService &m_auth;
    TaskRunner &m_runner;
    bool m_busy = false;
};

} // namespace presentation

#endif // PRESENTATION_LOGIN_LOGINPRESENTER_H
