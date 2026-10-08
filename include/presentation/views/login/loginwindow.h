#ifndef PRESENTATION_VIEWS_LOGIN_LOGINWINDOW_H
#define PRESENTATION_VIEWS_LOGIN_LOGINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

signals:
    void loginSuccessful();  // Señal que se emite cuando el login es exitoso
    // Se emite cuando el usuario intenta autenticarse (Enter o clic).
    // El controlador/backend real debe validar credenciales y llamar a
    // showLoginError() o cerrar/ocultar esta ventana según el resultado.
    void loginRequested(const QString &username, const QString &password);

public slots:
    // Muestra el layout de error oculto con el mensaje indicado.
    void showLoginError(const QString &message);
    // Oculta el mensaje de error (p.ej. cuando el usuario vuelve a escribir).
    void hideLoginError();
    // Habilita/deshabilita la interacción mientras se procesa el login
    // (evita doble envío y da feedback de "cargando").
    void setBusy(bool busy);

private slots:
    void onLoginClicked();
    void onFieldEdited();

private:
    void connectSignals();
    void setupPasswordToggle();
    bool validateFields();

    Ui::LoginWindow *ui;
};
#endif // PRESENTATION_VIEWS_LOGIN_LOGINWINDOW_H
