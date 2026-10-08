#include "adapters/persistence/sqluserdirectory.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

SqlUserDirectory::SqlUserDirectory(ConnectionPool &pool)
    : m_pool(pool)
{
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
            query.prepare(QStringLiteral(
                "SELECT password_hash, display_name, role FROM users WHERE username = :username"));
            query.bindValue(QStringLiteral(":username"), username);
            if (!query.exec()) {
                failure = query.lastError().text();
            } else if (query.next()) {
                record = application::UserRecordDto{query.value(0).toString(),
                                                    query.value(1).toString(),
                                                    query.value(2).toString()};
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
