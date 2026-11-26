#ifndef PRESENTATION_SETTINGS_USERMANAGEMENTPRESENTER_H
#define PRESENTATION_SETTINGS_USERMANAGEMENTPRESENTER_H

#include "application/auth/dto/authdtos.h"
#include "application/settings/dto/useraccountdtos.h"

#include <QObject>

#include <functional>
#include <optional>

namespace application {
class UserAdministrationService;
}

namespace presentation {

class IUserManagementView;
class TaskRunner;

// Presenter de la gestión de usuarios de Configuraciones (US-07.1): lista las
// cuentas, abre el formulario de alta, de edición o de contraseña, guarda lo
// capturado, y activa o desactiva la seleccionada. Todo lo que toca la base
// corre fuera del hilo de la interfaz (también el hash de la contraseña, que
// es lento a propósito).
//
// Sin widgets (solo QtCore): se prueba con una vista falsa.
class UserManagementPresenter final : public QObject
{
    Q_OBJECT

public:
    UserManagementPresenter(IUserManagementView &view,
                            const application::UserAdministrationService &service,
                            TaskRunner &runner, const application::SessionDto &actor,
                            QObject *parent = nullptr);

    void start();
    // Vuelve a leer la lista, salvo mientras guarda.
    void refresh();

    void onAddUser();
    void onEditUser();
    void onResetPassword();
    // Desactiva la seleccionada si está activa; si no, la reactiva.
    void onToggleActive();

    // Aceptar en cada modo del formulario.
    void onSubmitNewUser(const application::NewUserAccountDto &user);
    void onSubmitUserEdit(const application::UserAccountUpdateDto &user);
    void onSubmitPasswordReset(const application::PasswordResetDto &reset);

private:
    // Una lectura que sale bien borra el error de una anterior, salvo con
    // `keepMessage`: la que lanza un conflicto deja visible su aviso.
    void load(bool keepMessage = false);
    // La cuenta seleccionada, o nullopt (y un aviso) si no hay.
    std::optional<application::UserAccountDto> selectedUser();
    // Corre una escritura y atiende su resultado. `fromForm`: si viene del
    // formulario (sus errores van ahí) o de un botón de la tabla.
    void write(std::function<application::UserAccountWriteResult()> work, const QString &done,
               bool fromForm);

    IUserManagementView &m_view;
    const application::UserAdministrationService &m_service;
    TaskRunner &m_runner;
    const application::SessionDto m_actor;
    QList<application::UserAccountDto> m_users;
    bool m_saving = false;
    // Cada lectura lleva un número; solo se pinta la última.
    int m_loadGeneration = 0;
};

} // namespace presentation

#endif // PRESENTATION_SETTINGS_USERMANAGEMENTPRESENTER_H
