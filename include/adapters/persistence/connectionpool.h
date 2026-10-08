#ifndef ADAPTERS_PERSISTENCE_CONNECTIONPOOL_H
#define ADAPTERS_PERSISTENCE_CONNECTIONPOOL_H

#include <QSemaphore>
#include <QSqlDatabase>
#include <QString>
#include <QThreadStorage>

// Configuración del pool. La arma la raíz de composición a partir del .env.
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
//
// Ya no es un singleton: lo crea la raíz de composición y lo recibe cada
// adaptador por constructor. Así las pruebas de integración arman el suyo y
// nadie depende de un estado global que hay que configurar antes de usar.
class ConnectionPool
{
public:
    explicit ConnectionPool(const DatabaseConfig &config);
    // Cierra la conexión del hilo que lo destruye (el de la interfaz). Las de
    // los hilos de trabajo se cierran solas cuando esos hilos terminan, y el
    // TaskRunner, que es su dueño, se destruye antes que el pool.
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool &) = delete;
    ConnectionPool &operator=(const ConnectionPool &) = delete;

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

    // Cierra y olvida la conexión del hilo actual, para que el siguiente
    // acquire() abra una nueva. Lo llaman los adaptadores cuando una operación
    // falla: si PostgreSQL se reinició, QPSQL sigue reportando la conexión
    // como abierta y, en un hilo que vive mucho (el pool de tareas), fallaría
    // en cada intento. Hay que llamarlo cuando ya no quede ninguna QSqlQuery
    // viva sobre esa conexión.
    void discardThreadConnection();

private:
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

#endif // ADAPTERS_PERSISTENCE_CONNECTIONPOOL_H
