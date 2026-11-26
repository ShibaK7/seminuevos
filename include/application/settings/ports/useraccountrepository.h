#ifndef APPLICATION_SETTINGS_PORTS_USERACCOUNTREPOSITORY_H
#define APPLICATION_SETTINGS_PORTS_USERACCOUNTREPOSITORY_H

#include "application/settings/dto/useraccountdtos.h"

#include <QList>
#include <QString>

namespace application {

// Puerto: las cuentas de usuario (tabla users), para administrarlas. Va aparte
// de UserDirectory, que se queda chico porque solo sirve al login.
//
// Cada escritura recibe quién la hace (actorId) y el adaptador vuelve a
// comprobar, dentro de la misma transacción, que siga siendo Administrador
// activo: otro administrador pudo haberle quitado el rol después de que
// abrió la pantalla. Y ninguna deja la base sin un Administrador activo.
class UserAccountRepository
{
public:
    struct NewUserRecord
    {
        QString username; // ya normalizado
        QString displayName;
        domain::UserRole role = domain::UserRole::SalesAgent;
        QString passwordHash;
    };

    struct Outcome
    {
        enum class Status {
            Ok,
            DuplicateUsername, // ya hay una cuenta con ese usuario
            NotFound,          // la cuenta ya no existe
            Conflict,          // su versión ya no es la que se leyó
            LastActiveAdmin,   // dejaría la base sin un Administrador activo
            ActorNotAllowed,   // quien la hace ya no es Administrador activo
            Failed,            // falló la base
        };
        Status status = Status::Failed;
        int id = -1;
        QString errorMessage; // en Failed
    };

    virtual ~UserAccountRepository() = default;

    // Todas las cuentas, activas o no, por nombre. Si la consulta falla
    // devuelve vacío y deja la causa en `error`.
    virtual QList<UserAccountDto> listUsers(QString *error) = 0;

    virtual Outcome createUser(const NewUserRecord &user, int actorId) = 0;
    virtual Outcome updateUser(const UserAccountUpdateDto &user, int actorId) = 0;
    virtual Outcome setActive(int userId, const QString &version, bool active, int actorId) = 0;
    virtual Outcome resetPassword(int userId, const QString &passwordHash, int actorId) = 0;

protected:
    UserAccountRepository() = default;
    UserAccountRepository(const UserAccountRepository &) = default;
    UserAccountRepository &operator=(const UserAccountRepository &) = default;
};

} // namespace application

#endif // APPLICATION_SETTINGS_PORTS_USERACCOUNTREPOSITORY_H
