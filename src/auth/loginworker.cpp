#include "auth/loginworker.h"
#include "adapters/security/passwordhasher.h"
#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

LoginWorker::LoginWorker(const QString &username, const QString &password, QObject *parent)
    : QThread(parent)
    , m_username(username)
    , m_password(password)
{
}

void LoginWorker::run()
{
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();

    if (!db.isOpen()) {
        emit failed(QStringLiteral("No se pudo conectar con la base de datos: %1").arg(db.lastError().text()));
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT password_hash, display_name, role FROM users WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), m_username);

    if (!query.exec()) {
        emit failed(QStringLiteral("Error al consultar el usuario: %1").arg(query.lastError().text()));
        return;
    }

    if (!query.next()) {
        emit failed(QStringLiteral("Usuario o contraseña incorrectos. Verifica tus datos."));
        return;
    }

    const QString storedHash = query.value(0).toString();
    const QString displayName = query.value(1).toString();
    const QString role = query.value(2).toString();

    if (!PasswordHasher::verify(m_password, storedHash)) {
        emit failed(QStringLiteral("Usuario o contraseña incorrectos. Verifica tus datos."));
        return;
    }

    emit succeeded(displayName, role);
}
