#include "adapters/persistence/settings/sqluseraccountrepository.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

using Outcome = application::UserAccountRepository::Outcome;
using Status = Outcome::Status;

Outcome withStatus(Status status, int id = -1)
{
    Outcome outcome;
    outcome.status = status;
    outcome.id = id;
    return outcome;
}

Outcome failed(const QString &message)
{
    Outcome outcome;
    outcome.status = Status::Failed;
    outcome.errorMessage = message;
    return outcome;
}

// Una corrección con versión que no tocó ningún renglón: o la cuenta ya no
// existe, o alguien la cambió desde que se leyó.
Outcome missingOrStale(QSqlDatabase &db, int userId)
{
    QSqlQuery exists(db);
    exists.prepare(QStringLiteral("SELECT 1 FROM users WHERE id = :id"));
    exists.bindValue(QStringLiteral(":id"), userId);
    if (!exists.exec())
        return failed(exists.lastError().text());
    return withStatus(exists.next() ? Status::Conflict : Status::NotFound);
}

// La versión de una cuenta: lo que un administrador edita (nombre, rol y si
// está activa). No es xmin porque cada login reescribe el renglón
// (last_login) y daría un conflicto sin que nadie la haya editado; tampoco
// incluye la contraseña, que se restablece sin versión.
const QString kVersion = QStringLiteral("md5(concat_ws('|', display_name, role, active))");

} // namespace

SqlUserAccountRepository::SqlUserAccountRepository(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::UserAccountDto> SqlUserAccountRepository::listUsers(QString *error)
{
    QList<application::UserAccountDto> users;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT id, username, display_name, role, active, created_at, last_login, %1 "
            "  FROM users ORDER BY lower(display_name), id")
                          .arg(kVersion));
        if (!db.isOpen() || !query.exec()) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                application::UserAccountDto user;
                user.id = query.value(0).toInt();
                user.username = query.value(1).toString();
                user.displayName = query.value(2).toString();
                // El CHECK de la tabla solo admite los dos roles.
                user.role = domain::userRoleFromDb(query.value(3).toString())
                                .value_or(domain::UserRole::SalesAgent);
                user.active = query.value(4).toBool();
                user.createdAt = query.value(5).toDateTime();
                user.lastLogin = query.value(6).toDateTime();
                user.version = query.value(7).toString();
                users << user;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        users.clear();
    } else if (error) {
        error->clear();
    }
    return users;
}

Outcome SqlUserAccountRepository::inAdministratorTransaction(
    int actorId, const std::function<Outcome(QSqlDatabase &)> &write)
{
    Outcome result;
    bool databaseFailed = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        const auto fail = [&](const QString &message) {
            databaseFailed = true;
            db.rollback();
            return failed(message);
        };

        if (!db.isOpen()) {
            databaseFailed = true;
            result = failed(QStringLiteral("No se pudo conectar con la base de datos: %1")
                                .arg(db.lastError().text()));
        } else if (!db.transaction()) {
            databaseFailed = true;
            result = failed(QStringLiteral("No se pudo iniciar la transacción: %1")
                                .arg(db.lastError().text()));
        } else {
            result = [&]() -> Outcome {
                // 1. Un administrador a la vez. La primera llave (la tabla) lo
                // aparta de cualquier otro candado consultivo.
                QSqlQuery lock(db);
                if (!lock.exec(QStringLiteral("SELECT pg_advisory_xact_lock("
                                              "CAST('users'::regclass AS integer), 0)")))
                    return fail(lock.lastError().text());

                // 2. Quien actúa sigue siendo Administrador activo.
                QSqlQuery actor(db);
                actor.prepare(QStringLiteral("SELECT 1 FROM users WHERE id = :id AND active "
                                             "AND role = :admin"));
                actor.bindValue(QStringLiteral(":id"), actorId);
                actor.bindValue(QStringLiteral(":admin"), domain::toDbString(domain::UserRole::Admin));
                if (!actor.exec())
                    return fail(actor.lastError().text());
                if (!actor.next()) {
                    db.rollback();
                    return withStatus(Status::ActorNotAllowed);
                }

                // 3. La escritura.
                const Outcome written = write(db);
                if (written.status == Status::Failed)
                    return fail(written.errorMessage);
                if (written.status != Status::Ok) {
                    db.rollback();
                    return written;
                }

                // 4. Que quede al menos un Administrador activo.
                QSqlQuery admins(db);
                admins.prepare(QStringLiteral("SELECT count(*) FROM users WHERE active AND role = :admin"));
                admins.bindValue(QStringLiteral(":admin"), domain::toDbString(domain::UserRole::Admin));
                if (!admins.exec() || !admins.next())
                    return fail(admins.lastError().text());
                if (admins.value(0).toInt() == 0) {
                    db.rollback();
                    return withStatus(Status::LastActiveAdmin);
                }

                if (!db.commit()) {
                    const QString message = QStringLiteral("No se pudo confirmar la transacción: %1")
                                                .arg(db.lastError().text());
                    return fail(message);
                }
                return written;
            }();
        }
    }
    // Con la conexión ya soltada, una que falló se descarta para que el
    // reintento abra otra.
    if (databaseFailed)
        m_pool.discardThreadConnection();
    return result;
}

Outcome SqlUserAccountRepository::createUser(const NewUserRecord &user, int actorId)
{
    return inAdministratorTransaction(actorId, [&user](QSqlDatabase &db) {
        // Las cuentas viejas pueden tener mayúsculas: "Admin" y "admin" serían
        // la misma para el login. Con el candado tomado, nadie la crea entre
        // esta consulta y el INSERT.
        QSqlQuery taken(db);
        taken.prepare(QStringLiteral("SELECT 1 FROM users WHERE lower(username) = lower(:username)"));
        taken.bindValue(QStringLiteral(":username"), user.username);
        if (!taken.exec())
            return failed(taken.lastError().text());
        if (taken.next())
            return withStatus(Status::DuplicateUsername);

        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO users (username, password_hash, display_name, role, active) "
            "VALUES (:username, :hash, :name, :role, TRUE) RETURNING id"));
        insert.bindValue(QStringLiteral(":username"), user.username);
        insert.bindValue(QStringLiteral(":hash"), user.passwordHash);
        insert.bindValue(QStringLiteral(":name"), user.displayName);
        insert.bindValue(QStringLiteral(":role"), domain::toDbString(user.role));
        if (!insert.exec() || !insert.next()) {
            // La restricción UNIQUE, por si la creó alguien que no pasa por
            // aquí (DevSeeder, un script).
            if (insert.lastError().nativeErrorCode() == QStringLiteral("23505"))
                return withStatus(Status::DuplicateUsername);
            return failed(insert.lastError().text());
        }
        return withStatus(Status::Ok, insert.value(0).toInt());
    });
}

Outcome SqlUserAccountRepository::updateUser(const application::UserAccountUpdateDto &user,
                                             int actorId)
{
    return inAdministratorTransaction(actorId, [&user](QSqlDatabase &db) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral(
            "UPDATE users SET display_name = :name, role = :role "
            " WHERE id = :id AND %1 = :version")
                          .arg(kVersion));
        update.bindValue(QStringLiteral(":name"), user.displayName);
        update.bindValue(QStringLiteral(":role"), domain::toDbString(user.role));
        update.bindValue(QStringLiteral(":id"), user.id);
        update.bindValue(QStringLiteral(":version"), user.version);
        if (!update.exec())
            return failed(update.lastError().text());
        if (update.numRowsAffected() != 1)
            return missingOrStale(db, user.id);
        return withStatus(Status::Ok, user.id);
    });
}

Outcome SqlUserAccountRepository::setActive(int userId, const QString &version, bool active,
                                            int actorId)
{
    return inAdministratorTransaction(actorId, [=](QSqlDatabase &db) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral(
            "UPDATE users SET active = :active WHERE id = :id AND %1 = :version")
                          .arg(kVersion));
        update.bindValue(QStringLiteral(":active"), active);
        update.bindValue(QStringLiteral(":id"), userId);
        update.bindValue(QStringLiteral(":version"), version);
        if (!update.exec())
            return failed(update.lastError().text());
        if (update.numRowsAffected() != 1)
            return missingOrStale(db, userId);
        return withStatus(Status::Ok, userId);
    });
}

Outcome SqlUserAccountRepository::resetPassword(int userId, const QString &passwordHash,
                                                int actorId)
{
    // Sin versión: una contraseña nueva no pisa nada que otro haya corregido.
    return inAdministratorTransaction(actorId, [&](QSqlDatabase &db) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral("UPDATE users SET password_hash = :hash WHERE id = :id"));
        update.bindValue(QStringLiteral(":hash"), passwordHash);
        update.bindValue(QStringLiteral(":id"), userId);
        if (!update.exec())
            return failed(update.lastError().text());
        if (update.numRowsAffected() != 1)
            return withStatus(Status::NotFound);
        return withStatus(Status::Ok, userId);
    });
}
