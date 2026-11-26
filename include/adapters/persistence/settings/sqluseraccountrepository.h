#ifndef ADAPTERS_PERSISTENCE_SETTINGS_SQLUSERACCOUNTREPOSITORY_H
#define ADAPTERS_PERSISTENCE_SETTINGS_SQLUSERACCOUNTREPOSITORY_H

#include "application/settings/ports/useraccountrepository.h"

#include <functional>

class ConnectionPool;
class QSqlDatabase;

// Adaptador del puerto UserAccountRepository sobre la tabla users (US-07.1).
//
// Cada escritura corre en una transacción que:
//   1. toma un candado consultivo único para la administración de usuarios,
//      así dos administradores no se cruzan (si A le quita el rol a B
//      mientras B se lo quita a A, el segundo ya no pasa);
//   2. vuelve a comprobar que quien actúa siga siendo Administrador activo;
//   3. escribe, con la versión del renglón (xmin) en el WHERE cuando corrige;
//   4. si ya no queda ningún Administrador activo, deshace todo.
// La versión cambia también cuando la cuenta inicia sesión (last_login): un
// cambio a una cuenta que acaba de entrar sale como conflicto y la pantalla
// relee.
class SqlUserAccountRepository final : public application::UserAccountRepository
{
public:
    explicit SqlUserAccountRepository(ConnectionPool &pool);

    QList<application::UserAccountDto> listUsers(QString *error) override;
    Outcome createUser(const NewUserRecord &user, int actorId) override;
    Outcome updateUser(const application::UserAccountUpdateDto &user, int actorId) override;
    Outcome setActive(int userId, const QString &version, bool active, int actorId) override;
    Outcome resetPassword(int userId, const QString &passwordHash, int actorId) override;

private:
    // Los pasos 1, 2 y 4 alrededor de `write`, que hace el paso 3.
    Outcome inAdministratorTransaction(int actorId,
                                       const std::function<Outcome(QSqlDatabase &)> &write);

    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_SETTINGS_SQLUSERACCOUNTREPOSITORY_H
