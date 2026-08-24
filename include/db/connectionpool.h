#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <QSemaphore>
#include <QSqlDatabase>
#include <QString>
#include <QThreadStorage>

// Configuración del pool. Llenar con ConnectionPool::configure() antes de la
// primera llamada a ConnectionPool::instance().
struct DatabaseConfig
{
    QString driverName = QStringLiteral("QPSQL");
    QString hostName;
    int port = 5432;
    QString databaseName;
    QString userName;
    QString password;
    int maxConnections = 8;
};

// Qt no trae un connection pool propio: según la documentación ("Threads and
// the SQL Module"), una conexión QSqlDatabase solo puede usarse desde el
// hilo que la creó. Esta clase respeta esa regla dándole a cada hilo que la
// use su propia conexión (creada una sola vez, con nombre único, y cacheada
// mientras el hilo viva vía QThreadStorage), mientras un QSemaphore limita
// cuántas conexiones pueden estar abiertas/en uso a la vez.
class ConnectionPool
{
public:
    static void configure(const DatabaseConfig &config);
    static ConnectionPool &instance();

    class Handle
    {
    public:
        explicit Handle(ConnectionPool &pool);
        ~Handle();

        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;

        QSqlDatabase &database();

    private:
        ConnectionPool &m_pool;
    };

    Handle acquire();

private:
    explicit ConnectionPool(const DatabaseConfig &config);

    QSqlDatabase &threadConnection();

    struct ThreadConnection
    {
        QSqlDatabase db;
        QString connectionName;
        ~ThreadConnection();
    };

    DatabaseConfig m_config;
    QSemaphore m_semaphore;
    QThreadStorage<ThreadConnection *> m_connections;
};

#endif // CONNECTIONPOOL_H
