#ifndef PRESENTATION_PRESENTERS_ILOGINVIEW_H
#define PRESENTATION_PRESENTERS_ILOGINVIEW_H

#include <QString>

namespace presentation {

// Lo que LoginPresenter necesita de la pantalla de inicio de sesión. Es una
// interfaz pura, sin QObject: la implementa LoginWindow (un widget) y, en las
// pruebas, una vista falsa que solo anota lo que se le pidió.
class ILoginView
{
public:
    virtual ~ILoginView() = default;

    virtual void setBusy(bool busy) = 0;
    virtual void showLoginError(const QString &message) = 0;

protected:
    ILoginView() = default;
    ILoginView(const ILoginView &) = default;
    ILoginView &operator=(const ILoginView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_ILOGINVIEW_H
