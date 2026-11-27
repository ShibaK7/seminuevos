#include "adapters/persistence/auth/sqluserdirectory.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

SqlUserDirectory::SqlUserDirectory(ConnectionPool &pool)
    : m_pool(pool)
{
}

bool SqlUserDirectory::recordSuccessfulLogin(int userId, QString *error)
{
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = :id"));
            query.bindValue(QStringLiteral(":id"), userId);
            if (!query.exec())
                failure = query.lastError().text();
        }
    }
    if (!failure.isEmpty())
        m_pool.discardThreadConnection();
    if (error)
        *error = failure;
    return failure.isEmpty();
}

std::optional<application::UserRecordDto> SqlUserDirectory::findByUsername(const QString &username,
                                                                            QString *error)
{
    QString failure;
    std::optional<application::UserRecordDto> record;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = QStringLiteral("no hay conexión con la base de datos (%1)").arg(db.lastError().text());
        } else {
            QSqlQuery query(db);
            // Sin distinguir mayúsculas: los usuarios nuevos se guardan en
            // minúsculas, y "Admin" no debe fallar por una tecla. Si una base
            // vieja tuviera dos que solo difieren en eso, gana el exacto.
            query.prepare(QStringLiteral(
                "SELECT password_hash, display_name, role, id, active, username FROM users "
                "WHERE lower(username) = lower(:username) "
                "ORDER BY (username = :exact) DESC, id LIMIT 1"));
            query.bindValue(QStringLiteral(":username"), username);
            query.bindValue(QStringLiteral(":exact"), username);
            if (!query.exec()) {
                failure = query.lastError().text();
            } else if (query.next()) {
                record = application::UserRecordDto{query.value(0).toString(),
                                                    query.value(1).toString(),
                                                    query.value(2).toString(),
                                                    query.value(3).toInt(),
                                                    query.value(4).toBool(),
                                                    query.value(5).toString()};
            }
        }
    }

    if (!failure.isEmpty()) {
        // Si la base se reinició, QPSQL sigue creyendo que la conexión de este
        // hilo está abierta. Se descarta para que el siguiente intento abra
        // una nueva en vez de fallar igual hasta que el hilo muera.
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    }
    return record;
}
