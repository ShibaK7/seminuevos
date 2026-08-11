#include "include/loginwindow.h"
#include "include/mainwindow.h"

#include <QApplication>
#include <QTimer>
#include <QTranslator>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Seminuevos");

    LoginWindow login;

    // Conectar login exitoso con animación
    QObject::connect(&login, &LoginWindow::loginRequested, [&login, &app](const QString &user, const QString &pass) {
        // Simula latencia de red/consulta para mostrar el estado "ocupado".
        QTimer::singleShot(600, [&login, user, pass, &app]() {
            const bool valido = (user == "admin" && pass == "1234");
            if (valido) {
                // Animación de desvanecimiento para login
                QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect();
                login.setGraphicsEffect(effect);

                QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
                anim->setDuration(500);
                anim->setStartValue(1.0);
                anim->setEndValue(0.0);
                anim->start();

                login.setBusy(false);
                // Abrir la ventana principal
                QObject::connect(anim, &QPropertyAnimation::finished, [&]() {
                    login.hide();
                    // Crear MainWindow solo después del login exitoso
                    MainWindow *mainWin = new MainWindow();
                    mainWin->showMaximized(); // ← Aquí se maximiza automáticamente

                    //  Cerrar la aplicación cuando se cierre MainWindow
                     QObject::connect(mainWin, &MainWindow::destroyed, &app, &QApplication::quit);

                    // Animación de aparición para mainWin
                    QGraphicsOpacityEffect *effect2 = new QGraphicsOpacityEffect();
                    mainWin->setGraphicsEffect(effect2);

                    QPropertyAnimation *anim2 = new QPropertyAnimation(effect2, "opacity");
                    anim2->setDuration(500);
                    anim2->setStartValue(0.0);
                    anim2->setEndValue(1.0);
                    anim2->start();
                });
            } else {
                login.showLoginError(
                    QStringLiteral("Usuario o contraseña incorrectos. Verifica tus datos."));
            }
        });
    });

    login.show();
    return app.exec();
}
