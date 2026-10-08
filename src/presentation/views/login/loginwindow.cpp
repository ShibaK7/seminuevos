#include "presentation/views/login/loginwindow.h"
#include <qgraphicseffect.h>
#include "ui_loginwindow.h"

#include <QStyle>
#include <QPropertyAnimation>

LoginWindow::LoginWindow(QWidget* parent)
    : QMainWindow(parent)
      , ui(new Ui::LoginWindow)
{
    ui->setupUi(this);

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
