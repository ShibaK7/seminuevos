#ifndef APPLICATION_SETTINGS_DTO_USERACCOUNTDTOS_H
#define APPLICATION_SETTINGS_DTO_USERACCOUNTDTOS_H

#include "domain/common/value_objects/enums.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDateTime>
#include <QList>
#include <QString>

namespace application {

// DTOs de la gestión de usuarios de Configuraciones (US-07.1). Nunca llevan
// el hash de la contraseña hacia la pantalla.

// Una cuenta, como la lista la muestra.
struct UserAccountDto
{
    int id = -1;
    QString username;
    QString displayName;
    domain::UserRole role = domain::UserRole::SalesAgent;
    bool active = true;
    QDateTime createdAt;
    QDateTime lastLogin; // inválida si nunca entró
    // La versión del renglón al leerse: una corrección con otra versión es
    // un conflicto (otro administrador la cambió).
    QString version;

    // Las llena el servicio desde las reglas del dominio, porque la pantalla
    // no puede preguntarles: si es la cuenta de quien está en sesión y qué se
    // le puede cambiar.
    bool isSelf = false;
    bool canChangeRole = false;
    bool canChangeActive = false;
};

struct UserAccountListResult
{
    enum class Status {
        Loaded, // users trae las cuentas, por nombre
        Denied, // el rol no administra usuarios
        Failed, // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    QList<UserAccountDto> users;
    QString errorMessage; // en Failed
};

// El alta: usuario, nombre, contraseña dos veces y rol.
struct NewUserAccountDto
{
    QString username;
    QString displayName;
    QString password;
    QString passwordConfirmation;
    domain::UserRole role = domain::UserRole::SalesAgent;
};

// Corregir el nombre o el rol. El usuario no cambia después del alta.
struct UserAccountUpdateDto
{
    int id = -1;
    QString version;
    QString displayName;
    domain::UserRole role = domain::UserRole::SalesAgent;
};

// Restablecer la contraseña de una cuenta.
struct PasswordResetDto
{
    int id = -1;
    QString password;
    QString passwordConfirmation;
};

struct UserAccountWriteResult
{
    enum class Status {
        Saved,    // id es la cuenta guardada
        Rejected, // validation dice qué campos corregir
        Denied,   // el rol no administra usuarios (o ya no es Administrador)
        Conflict, // otro administrador la cambió desde que se leyó
        NotFound, // la cuenta ya no existe
        Failed,   // falló la base y no se guardó nada; se puede reintentar
    };
    Status status = Status::Failed;
    int id = -1;
    domain::ValidationResult validation; // en Rejected
    QString errorMessage;                 // en Failed
};

} // namespace application

#endif // APPLICATION_SETTINGS_DTO_USERACCOUNTDTOS_H
