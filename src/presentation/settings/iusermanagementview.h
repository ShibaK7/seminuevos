#ifndef PRESENTATION_SETTINGS_IUSERMANAGEMENTVIEW_H
#define PRESENTATION_SETTINGS_IUSERMANAGEMENTVIEW_H

#include "application/settings/dto/useraccountdtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>

namespace presentation {

// La gestión de usuarios de Configuraciones (US-07.1), vista desde su
// presenter. La implementa UserManagementWidget.
class IUserManagementView
{
public:
    virtual ~IUserManagementView() = default;

    // Mientras se lee la lista o se guarda algo: los botones no responden.
    virtual void setBusy(bool busy) = 0;
    virtual void showUsers(const QList<application::UserAccountDto> &users) = 0;
    // El id de la cuenta seleccionada en la tabla, o -1 si no hay.
    virtual int selectedUserId() const = 0;

    // --- El formulario (modal; lo capturado llega después, con una señal) --
    virtual void askNewUser() = 0;
    // Nombre y rol; el rol bloqueado si no se le puede cambiar.
    virtual void askUserEdit(const application::UserAccountDto &user) = 0;
    virtual void askPasswordReset(const application::UserAccountDto &user) = 0;
    // Mientras se guarda: Aceptar no se puede volver a presionar.
    virtual void setUserFormBusy(bool busy) = 0;
    // Marca bajo cada campo lo que reportó el dominio; el formulario sigue
    // abierto.
    virtual void showUserFormErrors(const QList<domain::ValidationError> &errors) = 0;
    virtual void closeUserForm() = 0;

    // "¿Desactivar a ...?" / "¿Reactivar a ...?". true si acepta.
    virtual bool confirmActivation(const application::UserAccountDto &user, bool activate) = 0;
    virtual void showMessage(const QString &message) = 0;
    virtual void showNotice(const QString &notice) = 0;

protected:
    IUserManagementView() = default;
    IUserManagementView(const IUserManagementView &) = default;
    IUserManagementView &operator=(const IUserManagementView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_SETTINGS_IUSERMANAGEMENTVIEW_H
