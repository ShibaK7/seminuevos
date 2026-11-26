#include "presentation/settings/useraccountdialog.h"
#include "ui_useraccountdialog.h"

#include "presentation/common/forms/formsupport.h"

#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

UserAccountDialog::UserAccountDialog(Mode mode, const application::UserAccountDto &account,
                                     QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UserAccountDialog)
    , m_mode(mode)
    , m_account(account)
{
    ui->setupUi(this);
    formsupport::addFieldErrorLabels(this);

    for (const domain::UserRole role : domain::allUserRoles())
        ui->roleCombo->addItem(domain::displayLabel(role), static_cast<int>(role));
    formsupport::selectEnum(ui->roleCombo, account.role);
    ui->usernameEdit->setText(account.username);
    ui->displayNameEdit->setText(account.displayName);

    const auto showRow = [this](QWidget *label, QWidget *field, bool visible) {
        label->setVisible(visible);
        field->setVisible(visible);
    };
    switch (mode) {
    case Mode::Create:
        ui->titleLabel->setText(QStringLiteral("Nuevo usuario"));
        break;
    case Mode::Edit:
        ui->titleLabel->setText(QStringLiteral("Editar usuario"));
        // El usuario no cambia después del alta.
        ui->usernameEdit->setEnabled(false);
        showRow(ui->passwordLabel, ui->passwordEdit, false);
        showRow(ui->passwordConfirmationLabel, ui->passwordConfirmationEdit, false);
        if (!account.canChangeRole) {
            ui->roleCombo->setEnabled(false);
            ui->roleCombo->setToolTip(QStringLiteral("No puedes cambiar tu propio rol."));
        }
        break;
    case Mode::Password:
        ui->titleLabel->setText(QStringLiteral("Restablecer contraseña"));
        ui->usernameEdit->setEnabled(false);
        showRow(ui->displayNameLabel, ui->displayNameEdit, false);
        showRow(ui->roleLabel, ui->roleCombo, false);
        ui->passwordLabel->setText(QStringLiteral("Contraseña nueva:"));
        break;
    }

    connect(ui->acceptButton, &QPushButton::clicked, this, &UserAccountDialog::submitted);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

UserAccountDialog::~UserAccountDialog()
{
    delete ui;
}

UserAccountDialog::Mode UserAccountDialog::mode() const
{
    return m_mode;
}

application::NewUserAccountDto UserAccountDialog::newUser() const
{
    application::NewUserAccountDto user;
    user.username = ui->usernameEdit->text();
    user.displayName = ui->displayNameEdit->text();
    user.password = ui->passwordEdit->text();
    user.passwordConfirmation = ui->passwordConfirmationEdit->text();
    user.role = static_cast<domain::UserRole>(ui->roleCombo->currentData().toInt());
    return user;
}

application::UserAccountUpdateDto UserAccountDialog::userUpdate() const
{
    application::UserAccountUpdateDto user;
    user.id = m_account.id;
    user.version = m_account.version;
    user.displayName = ui->displayNameEdit->text();
    user.role = static_cast<domain::UserRole>(ui->roleCombo->currentData().toInt());
    return user;
}

application::PasswordResetDto UserAccountDialog::passwordReset() const
{
    application::PasswordResetDto reset;
    reset.id = m_account.id;
    reset.password = ui->passwordEdit->text();
    reset.passwordConfirmation = ui->passwordConfirmationEdit->text();
    return reset;
}

void UserAccountDialog::setBusy(bool busy)
{
    m_busy = busy;
    ui->acceptButton->setEnabled(!busy);
    ui->cancelButton->setEnabled(!busy);
}

void UserAccountDialog::reject()
{
    // Mientras guarda no se cierra (Cancelar, Esc o la X pasan por aquí):
    // el resultado, sea un error o el aviso de que se guardó, necesita el
    // formulario abierto para tener sentido.
    if (m_busy)
        return;
    QDialog::reject();
}

void UserAccountDialog::showErrors(const QList<domain::ValidationError> &errors)
{
    const QList<domain::ValidationError> unshown = formsupport::showFieldErrors(this, errors);
    // Un mensaje largo ocupa varias líneas: el diálogo crece para que se lea
    // completo (el tamaño del .ui solo alcanza para el formulario sin errores).
    layout()->activate();
    if (layout()->hasHeightForWidth())
        resize(width(), qMax(height(), layout()->totalHeightForWidth(width())));
    for (const domain::ValidationError &error : errors) {
        if (formsupport::focusField(this, error.field))
            break;
    }
    if (!unshown.isEmpty()) {
        QStringList messages;
        for (const domain::ValidationError &error : unshown)
            messages << error.message;
        QMessageBox::warning(this, windowTitle(), messages.join(QStringLiteral("\n")));
    }
}
