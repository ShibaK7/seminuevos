#include "presentation/login/loginwindow.h"
#include <qgraphicseffect.h>
#include "ui_loginwindow.h"
#include "presentation/common/forms/formsupport.h"

#include <QStyle>
#include <QPropertyAnimation>
#include <QScreen>

namespace {

// Lo que se le deja a Windows alrededor de la ventana: la barra de título y
// los bordes. Antes de mostrarla todavía no se sabe cuánto miden exactamente.
const QSize kWindowFrame(16, 48);

// El .ui la diseña de 700x600 y no la deja crecer más. En una pantalla donde
// así no cabe (una laptop de 1024x600, o una de 1366x768 al 125 %) arranca
// más chica, arriba y centrada: su contenido se acomoda solo.
void fitToScreen(QWidget *window)
{
    const QScreen *screen = window->screen();
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    const QSize room = area.size() - kWindowFrame;
    if (window->width() <= room.width() && window->height() <= room.height())
        return;
    window->resize(window->size().boundedTo(room));
    window->move(area.x() + (area.width() - window->width()) / 2, area.y());
}

} // namespace

LoginWindow::LoginWindow(QWidget* parent)
    : QMainWindow(parent)
      , ui(new Ui::LoginWindow)
{
    ui->setupUi(this);
    fitToScreen(this);
    // Su hoja propia (resources/styles/login.qss): el login se muestra antes
    // que la hoja global, que pisaría su fondo.
    setStyleSheet(formsupport::styleSheetResource(QStringLiteral(":/styles/login.qss")));

    // Sombra para dar jerarquía visual a la tarjeta sobre el fondo.
    // (Este tipo de efecto no se define en el .ui, se agrega en código.)
    auto* shadow = new QGraphicsDropShadowEffect(ui->panel_login);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(16, 27, 51, 60));
    ui->panel_login->setGraphicsEffect(shadow);

    setupPasswordToggle();
    connectSignals();

    ui->usernameEdit->setFocus();
    ui->errorBox->setVisible(false);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::setupPasswordToggle()
{
    // Icono para mostrar/ocultar la contraseña sin salir del campo.
    QAction* toggleEcho = ui->passwordEdit->addAction(
        style()->standardIcon(QStyle::SP_DialogYesButton),
        QLineEdit::TrailingPosition);
    toggleEcho->setToolTip(QStringLiteral("Mostrar/ocultar contraseña"));
    connect(toggleEcho, &QAction::triggered, this, [this]()
    {
        const bool hidden = ui->passwordEdit->echoMode() == QLineEdit::Password;
        ui->passwordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    });

    ui->errorIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(16, 16));
}

void LoginWindow::connectSignals()
{
    // ---------- Interacción: Enter dispara login desde cualquier campo ----------
    connect(ui->usernameEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);

    // El error se oculta en cuanto el usuario empieza a corregir los datos,
    // para no dejar mensajes obsoletos en pantalla (buena práctica de UX).
    connect(ui->usernameEdit, &QLineEdit::textEdited, this, &LoginWindow::onFieldEdited);
    connect(ui->passwordEdit, &QLineEdit::textEdited, this, &LoginWindow::onFieldEdited);
}

void LoginWindow::onFieldEdited()
{
    hideLoginError();
}

bool LoginWindow::validateFields()
{
    const QString user = ui->usernameEdit->text().trimmed();
    const QString pass = ui->passwordEdit->text();

    if (user.isEmpty() || pass.isEmpty())
    {
        showLoginError(QStringLiteral("Ingresa tu usuario y contraseña para continuar."));
        return false;
    }
    return true;
}

void LoginWindow::onLoginClicked()
{
    if (!validateFields())
        return;

    hideLoginError();
    setBusy(true);

    // La ventana solo recopila y valida el formato de los datos; la
    // autenticación real (contra API/base de datos) la resuelve quien
    // escuche esta señal, que luego debe llamar a showLoginError() o
    // continuar el flujo (p.ej. cerrar esta ventana) según el resultado.
    emit loginRequested(ui->usernameEdit->text().trimmed(), ui->passwordEdit->text());
}

void LoginWindow::showLoginError(const QString& message)
{
    ui->errorLabel->setText(message);
    if (!ui->errorBox->isVisible())
    {
        ui->errorBox->setVisible(true);

        // Pequeña animación de aparición para que el error no "salte"
        // de forma abrupta en la interfaz.
        auto* effect = new QGraphicsOpacityEffect(ui->errorBox);
        ui->errorBox->setGraphicsEffect(effect);
        auto* anim = new QPropertyAnimation(effect, "opacity", ui->errorBox);
        anim->setDuration(180);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    setBusy(false);
}

void LoginWindow::hideLoginError()
{
    if (ui->errorBox->isVisible())
        ui->errorBox->setVisible(false);
}

void LoginWindow::setBusy(bool busy)
{
    ui->loginButton->setEnabled(!busy);
    ui->usernameEdit->setEnabled(!busy);
    ui->passwordEdit->setEnabled(!busy);
    ui->loginButton->setText(busy ? QStringLiteral("Iniciando sesión...") : QStringLiteral("Iniciar sesión"));
}
