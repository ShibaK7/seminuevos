#include "adapters/persistence/connectionpool.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlError>
#include <QThread>

ConnectionPool::ConnectionPool(const DatabaseConfig &config)
    : m_config(config)
    , m_semaphore(config.maxConnections)
{
}

ConnectionPool::~ConnectionPool()
{
    // QThreadStorage no borra lo que guarda al destruirse: sin esto, la
    // conexión del hilo de la interfaz quedaría abierta y registrada en Qt.
    discardThreadConnection();
}

ConnectionPool::ThreadConnection::~ThreadConnection()
{
    const QString name = connectionName;
    db.close();
    db = QSqlDatabase(); // soltar nuestra referencia antes de removeDatabase()
    QSqlDatabase::removeDatabase(name);
}

QSqlDatabase &ConnectionPool::threadConnection()
{
    if (!m_connections.hasLocalData()) {
        auto *conn = new ThreadConnection;
        conn->connectionName = QStringLiteral("pool_%1")
                                    .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        conn->db = QSqlDatabase::addDatabase(m_config.driverName, conn->connectionName);
        conn->db.setHostName(m_config.hostName);
        conn->db.setPort(m_config.port);
        conn->db.setDatabaseName(m_config.databaseName);
        conn->db.setUserName(m_config.userName);
        conn->db.setPassword(m_config.password);
        m_connections.setLocalData(conn);
    }

    ThreadConnection *conn = m_connections.localData();
    if (!conn->db.isOpen() && !conn->db.open()) {
        qWarning() << "ConnectionPool: no se pudo abrir la conexión" << conn->connectionName
                    << conn->db.lastError().text();
    }
    return conn->db;
}

ConnectionPool::Handle ConnectionPool::acquire()
{
    // Solo en Debug: así un descuido aparece en cuanto se prueba la pantalla,
    // con el lugar exacto en la pila, en vez de como una congelada ocasional.
    const QCoreApplication *app = QCoreApplication::instance();
    Q_ASSERT_X(m_guiThreadAllowed || !app || QThread::currentThread() != app->thread(),
               "ConnectionPool::acquire",
               "SQL en el hilo de la interfaz: corre la consulta con el TaskRunner");
    Q_UNUSED(app);
    return Handle(*this);
}

void ConnectionPool::setGuiThreadAllowed(bool allowed)
{
    m_guiThreadAllowed = allowed;
}

void ConnectionPool::discardThreadConnection()
{
    // setLocalData(nullptr) borra la ThreadConnection anterior, cuyo
    // destructor cierra la conexión y la quita del registro de Qt.
    if (m_connections.hasLocalData())
        m_connections.setLocalData(nullptr);
}

ConnectionPool::Handle::Handle(ConnectionPool &pool)
    : m_pool(pool)
{
    m_pool.m_semaphore.acquire();
}

ConnectionPool::Handle::~Handle()
{
    m_pool.m_semaphore.release();
}

QSqlDatabase &ConnectionPool::Handle::database()
{
    return m_pool.threadConnection();
}
