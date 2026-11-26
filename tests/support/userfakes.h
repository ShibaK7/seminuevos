#ifndef TESTS_SUPPORT_USERFAKES_H
#define TESTS_SUPPORT_USERFAKES_H

// Puerto falso de las cuentas de usuario (US-07.1). Va aparte de fakes.h para
// que las pruebas de los demás módulos no dependan de él.

#include "application/settings/ports/useraccountrepository.h"

#include <QMap>

#include <optional>

namespace fakes {

// La tabla users en memoria, con las mismas garantías que el adaptador: quien
// actúa debe seguir siendo Administrador activo, nunca se queda la base sin
// uno, el usuario no se repite (sin importar mayúsculas) y una versión vieja
// es un conflicto.
class InMemoryUserAccountRepository final : public application::UserAccountRepository
{
public:
    struct Row
    {
        application::UserAccountDto account;
        QString passwordHash;
    };
    QMap<int, Row> rows;
    // Si no está vacío, todo "falla" con este error.
    QString failure;
    // Si tiene valor, toda escritura responde esto sin cambiar nada: para lo
    // que el servicio no puede provocar solo.
    std::optional<Outcome::Status> forced;
    int writes = 0;

    int add(const QString &username, domain::UserRole role, bool active = true)
    {
        const int id = rows.isEmpty() ? 1 : rows.lastKey() + 1;
        application::UserAccountDto account;
        account.id = id;
        account.username = username;
        account.displayName = username.toUpper();
        account.role = role;
        account.active = active;
        account.version = QStringLiteral("v1");
        rows.insert(id, {account, QStringLiteral("plain:inicial")});
        return id;
    }

    QList<application::UserAccountDto> listUsers(QString *error) override
    {
        if (error)
            *error = failure;
        QList<application::UserAccountDto> users;
        if (failure.isEmpty()) {
            for (const Row &row : std::as_const(rows))
                users << row.account;
        }
        return users;
    }

    Outcome createUser(const NewUserRecord &user, int actorId) override
    {
        ++writes;
        Outcome outcome = check(actorId);
        if (outcome.status != Outcome::Status::Ok)
            return outcome;
        for (const Row &row : std::as_const(rows)) {
            if (row.account.username.compare(user.username, Qt::CaseInsensitive) == 0)
                return status(Outcome::Status::DuplicateUsername);
        }
        const int id = add(user.username, user.role);
        rows[id].account.displayName = user.displayName;
        rows[id].passwordHash = user.passwordHash;
        outcome.id = id;
        return outcome;
    }

    Outcome updateUser(const application::UserAccountUpdateDto &user, int actorId) override
    {
        ++writes;
        Outcome outcome = checkTarget(actorId, user.id, user.version);
        if (outcome.status != Outcome::Status::Ok)
            return outcome;
        Row before = rows.value(user.id);
        rows[user.id].account.displayName = user.displayName;
        rows[user.id].account.role = user.role;
        return commit(user.id, before);
    }

    Outcome setActive(int userId, const QString &version, bool active, int actorId) override
    {
        ++writes;
        Outcome outcome = checkTarget(actorId, userId, version);
        if (outcome.status != Outcome::Status::Ok)
            return outcome;
        Row before = rows.value(userId);
        rows[userId].account.active = active;
        return commit(userId, before);
    }

    Outcome resetPassword(int userId, const QString &passwordHash, int actorId) override
    {
        ++writes;
        Outcome outcome = check(actorId);
        if (outcome.status != Outcome::Status::Ok)
            return outcome;
        if (!rows.contains(userId))
            return status(Outcome::Status::NotFound);
        rows[userId].passwordHash = passwordHash;
        outcome.id = userId;
        return outcome;
    }

private:
    static Outcome status(Outcome::Status value)
    {
        Outcome outcome;
        outcome.status = value;
        return outcome;
    }

    Outcome check(int actorId) const
    {
        if (forced)
            return status(*forced);
        if (!failure.isEmpty()) {
            Outcome outcome;
            outcome.errorMessage = failure;
            return outcome;
        }
        const auto actor = rows.constFind(actorId);
        if (actor == rows.cend() || !actor->account.active
            || actor->account.role != domain::UserRole::Admin)
            return status(Outcome::Status::ActorNotAllowed);
        return status(Outcome::Status::Ok);
    }

    Outcome checkTarget(int actorId, int userId, const QString &version) const
    {
        Outcome outcome = check(actorId);
        if (outcome.status != Outcome::Status::Ok)
            return outcome;
        if (!rows.contains(userId))
            return status(Outcome::Status::NotFound);
        if (rows.value(userId).account.version != version)
            return status(Outcome::Status::Conflict);
        return outcome;
    }

    // Deshace el cambio si dejó la base sin un Administrador activo.
    Outcome commit(int userId, const Row &before)
    {
        bool anyAdmin = false;
        for (const Row &row : std::as_const(rows))
            anyAdmin = anyAdmin || (row.account.active && row.account.role == domain::UserRole::Admin);
        if (!anyAdmin) {
            rows[userId] = before;
            return status(Outcome::Status::LastActiveAdmin);
        }
        rows[userId].account.version = QStringLiteral("v%1").arg(++m_versions + 1);
        Outcome outcome = status(Outcome::Status::Ok);
        outcome.id = userId;
        return outcome;
    }

    int m_versions = 0;
};

} // namespace fakes

#endif // TESTS_SUPPORT_USERFAKES_H
