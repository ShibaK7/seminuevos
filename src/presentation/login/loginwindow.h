#ifndef PRESENTATION_LOGIN_LOGINWINDOW_H
#define PRESENTATION_LOGIN_LOGINWINDOW_H

#include "presentation/login/iloginview.h"

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

// Implementa ILoginView: LoginPresenter le dice qué mostrar sin conocer el
// widget.
class LoginWindow : public QMainWindow, public presentation::ILoginView
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

signals:
    // Se emite cuando el usuario intenta autenticarse (Enter o clic).
    // LoginPresenter recibe el intento y llama a
    // showLoginError() o avisa a la raíz de composición si se autenticó.
    void loginRequested(const QString &username, const QString &password);

public slots:
    // Muestra el layout de error oculto con el mensaje indicado.
    void showLoginError(const QString &message) override;
    // Oculta el mensaje de error (p.ej. cuando el usuario vuelve a escribir).
    void hideLoginError();
    // Habilita/deshabilita la interacción mientras se procesa el login
    // (evita doble envío y da feedback de "cargando").
    void setBusy(bool busy) override;

private slots:
    void onLoginClicked();
    void onFieldEdited();

private:
    void connectSignals();
    void setupPasswordToggle();
    bool validateFields();

    Ui::LoginWindow *ui;
};
#endif // PRESENTATION_LOGIN_LOGINWINDOW_H
