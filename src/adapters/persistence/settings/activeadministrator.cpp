#include "adapters/persistence/settings/activeadministrator.h"

#include "domain/common/value_objects/enums.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool isActiveAdministrator(QSqlDatabase &db, int actorId, QString *error)
{
    QSqlQuery actor(db);
    actor.prepare(QStringLiteral("SELECT 1 FROM users WHERE id = :id AND active "
                                 "AND role = :admin FOR SHARE"));
    actor.bindValue(QStringLiteral(":id"), actorId);
    actor.bindValue(QStringLiteral(":admin"), domain::toDbString(domain::UserRole::Admin));
    if (!actor.exec()) {
        if (error)
            *error = actor.lastError().text();
        return false;
    }
    if (error)
        error->clear();
    return actor.next();
}
