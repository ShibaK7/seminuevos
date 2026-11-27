#ifndef PRESENTATION_SETTINGS_USERACCOUNTDIALOG_H
#define PRESENTATION_SETTINGS_USERACCOUNTDIALOG_H

#include "application/settings/dto/useraccountdtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
    class UserAccountDialog;
}
QT_END_NAMESPACE

// El formulario modal de una cuenta de usuario (US-07.1), en tres modos:
//   - Create: usuario, nombre, rol y contraseña dos veces;
//   - Edit: nombre y rol (el usuario no cambia, y el rol queda bloqueado en
//     la propia cuenta);
//   - Password: la contraseña nueva dos veces.
// Su formulario es useraccountdialog.ui, en esta misma carpeta.
//
// Vista pasiva: Aceptar no valida ni cierra, solo avisa (submitted). Quien
// guarda decide si cierra (accept()) o marca los errores que reportó el
// dominio (showErrors()). Cancelar sí lo cierra solo, salvo mientras guarda.
class UserAccountDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Create, Edit, Password };

    UserAccountDialog(Mode mode, const application::UserAccountDto &account, QWidget *parent = nullptr);
    ~UserAccountDialog() override;

    Mode mode() const;
    application::NewUserAccountDto newUser() const;
    application::UserAccountUpdateDto userUpdate() const;
    application::PasswordResetDto passwordReset() const;

    // Mientras guarda, ni Aceptar ni Cancelar, y el diálogo no se cierra.
    void setBusy(bool busy);
    // Marca cada campo con su error y le da el foco al primero.
    void showErrors(const QList<domain::ValidationError> &errors);

public slots:
    void reject() override;

signals:
    // Aceptar.
    void submitted();

private:
    Ui::UserAccountDialog *ui;
    bool m_busy = false;
    Mode m_mode;
    application::UserAccountDto m_account;
};

#endif // PRESENTATION_SETTINGS_USERACCOUNTDIALOG_H
