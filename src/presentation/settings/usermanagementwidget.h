#ifndef PRESENTATION_SETTINGS_USERMANAGEMENTWIDGET_H
#define PRESENTATION_SETTINGS_USERMANAGEMENTWIDGET_H

#include "presentation/settings/iusermanagementview.h"

#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class UserManagementWidget;
}
QT_END_NAMESPACE

class UserAccountDialog;

namespace presentation {
class UserManagementPresenter;
}

// La tarjeta de usuarios de Configuraciones (US-07.1): la tabla de cuentas
// (Usuario, Nombre, Rol, Estado y Último acceso), "Nuevo usuario", y para la
// seleccionada "Editar", "Restablecer contraseña" y "Desactivar" o
// "Reactivar". Su formulario es usermanagementwidget.ui, en esta misma
// carpeta.
//
// Vista pasiva de UserManagementPresenter. El formulario de cada cuenta es
// UserAccountDialog.
class UserManagementWidget : public QWidget, public presentation::IUserManagementView
{
    Q_OBJECT

public:
    enum Column { UsernameColumn, NameColumn, RoleColumn, StatusColumn, LastLoginColumn };

    explicit UserManagementWidget(QWidget *parent = nullptr);
    ~UserManagementWidget() override;

    void bind(presentation::UserManagementPresenter &presenter);

    // --- IUserManagementView ---
    void setBusy(bool busy) override;
    void showUsers(const QList<application::UserAccountDto> &users) override;
    int selectedUserId() const override;
    void askNewUser() override;
    void askUserEdit(const application::UserAccountDto &user) override;
    void askPasswordReset(const application::UserAccountDto &user) override;
    void setUserFormBusy(bool busy) override;
    void showUserFormErrors(const QList<domain::ValidationError> &errors) override;
    void closeUserForm() override;
    bool confirmActivation(const application::UserAccountDto &user, bool activate) override;
    void showMessage(const QString &message) override;
    void showNotice(const QString &notice) override;

signals:
    void addUserRequested();
    void editUserRequested();
    void resetPasswordRequested();
    void toggleActiveRequested();
    void newUserSubmitted(const application::NewUserAccountDto &user);
    void userEditSubmitted(const application::UserAccountUpdateDto &user);
    void passwordResetSubmitted(const application::PasswordResetDto &reset);

private:
    void openForm(UserAccountDialog *form);
    // "Desactivar" o "Reactivar" según la cuenta seleccionada.
    void updateSelectionButtons();

    Ui::UserManagementWidget *ui;
    QList<application::UserAccountDto> m_users;
    // El formulario abierto, si hay uno. Se borra solo al cerrarse.
    QPointer<UserAccountDialog> m_form;
    bool m_busy = false;
};

#endif // PRESENTATION_SETTINGS_USERMANAGEMENTWIDGET_H
