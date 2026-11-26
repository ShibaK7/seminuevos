// Prueba de integración de las cuentas de usuario (US-07.1) contra el
// PostgreSQL de Docker. Es opcional: solo se compila con
// -DSEMINUEVOS_DB_TESTS=ON y necesita `docker compose up -d`.
//
// Trabaja en una base desechable (seminuevos_tst_users) que crea con el
// 00_schema.sql del repositorio y borra al terminar: así puede quitarle el
// rol a los administradores sin tocar las cuentas de la base de desarrollo.

#include "adapters/persistence/auth/sqluserdirectory.h"
#include "adapters/persistence/commercial/sqlsalerepository.h"
#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/schemaupgrade.h"
#include "adapters/persistence/settings/sqluseraccountrepository.h"

#include <QDeadlineTimer>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QThread>
#include <QtTest>

#include <atomic>
#include <memory>

using Outcome = application::UserAccountRepository::Outcome;
using OutcomeStatus = Outcome::Status;
using domain::UserRole;

namespace {

const QString kScratchDatabase = QStringLiteral("seminuevos_tst_users");

QMap<QString, QString> readEnv()
{
    QMap<QString, QString> values;
    QFile file(QStringLiteral(SEMINUEVOS_SOURCE_DIR) + QStringLiteral("/.env"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            const int eq = line.indexOf(QLatin1Char('='));
            if (!line.startsWith(QLatin1Char('#')) && eq > 0)
                values.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
        }
    }
    return values;
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

} // namespace

class TstUsersDb : public QObject
{
    Q_OBJECT

private:
    DatabaseConfig m_config;
    // La conexión a la base de desarrollo, solo para crear y borrar la
    // desechable. Va con su propio nombre y no con otro ConnectionPool: dos
    // pools en el mismo hilo usarían el mismo nombre de conexión.
    const QString m_adminConnection = QStringLiteral("tst-users-admin");
    // La desechable.
    std::unique_ptr<ConnectionPool> m_pool;
    int m_adminId = -1;
    int m_sellerId = -1;

    bool exec(ConnectionPool &pool, const QString &sql)
    {
        ConnectionPool::Handle handle = pool.acquire();
        QSqlQuery query(handle.database());
        if (!query.exec(sql)) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return false;
        }
        return true;
    }

    bool execOnDevelopment(const QString &sql)
    {
        QSqlQuery query(QSqlDatabase::database(m_adminConnection));
        if (!query.exec(sql)) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return false;
        }
        return true;
    }

    int insertUser(const QString &username, UserRole role, bool active = true)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(QStringLiteral(
            "INSERT INTO users (username, password_hash, display_name, role, active) "
            "VALUES (:username, 'x', :name, :role, :active) RETURNING id"));
        query.bindValue(QStringLiteral(":username"), username);
        query.bindValue(QStringLiteral(":name"), username.toUpper());
        query.bindValue(QStringLiteral(":role"), domain::toDbString(role));
        query.bindValue(QStringLiteral(":active"), active);
        if (!query.exec() || !query.next()) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return -1;
        }
        return query.value(0).toInt();
    }

    application::UserAccountDto account(int id)
    {
        SqlUserAccountRepository accounts(*m_pool);
        QString error;
        for (const application::UserAccountDto &user : accounts.listUsers(&error)) {
            if (user.id == id)
                return user;
        }
        return {};
    }

private slots:
    void initTestCase()
    {
        const QMap<QString, QString> env = readEnv();
        m_config.hostName = env.value(QStringLiteral("POSTGRES_HOST"), QStringLiteral("localhost"));
        m_config.port = env.value(QStringLiteral("POSTGRES_PORT"), QStringLiteral("5432")).toInt();
        m_config.databaseName = env.value(QStringLiteral("POSTGRES_DB"), QStringLiteral("seminuevos"));
        m_config.userName = env.value(QStringLiteral("POSTGRES_USER"), QStringLiteral("seminuevos_app"));
        m_config.password = env.value(QStringLiteral("POSTGRES_PASSWORD"));
        {
            QSqlDatabase development = QSqlDatabase::addDatabase(m_config.driverName, m_adminConnection);
            development.setHostName(m_config.hostName);
            development.setPort(m_config.port);
            development.setDatabaseName(m_config.databaseName);
            development.setUserName(m_config.userName);
            development.setPassword(m_config.password);
            if (!development.open())
                QSKIP("PostgreSQL no está disponible (docker compose up -d).");
        }
        execOnDevelopment(QStringLiteral("DROP DATABASE IF EXISTS %1 WITH (FORCE)").arg(kScratchDatabase));
        if (!execOnDevelopment(QStringLiteral("CREATE DATABASE %1").arg(kScratchDatabase)))
            QSKIP("El usuario de la base no puede crear una base desechable.");

        DatabaseConfig scratch = m_config;
        scratch.databaseName = kScratchDatabase;
        m_pool = std::make_unique<ConnectionPool>(scratch);
        m_pool->setGuiThreadAllowed(true);
        const QString schema = readFile(QStringLiteral(SEMINUEVOS_SOURCE_DIR) + QStringLiteral("/init-db/00_schema.sql"));
        QVERIFY(!schema.isEmpty());
        QVERIFY(exec(*m_pool, schema));

        // Una base de antes de la US-07.1 (y del flujo de ventas): sin esas
        // dos columnas. La app las agrega al arrancar.
        QVERIFY(exec(*m_pool, QStringLiteral("ALTER TABLE users DROP COLUMN active, DROP COLUMN last_login")));
        {
            ConnectionPool::Handle handle = m_pool->acquire();
            const SchemaUpgrade::Result upgrade = SchemaUpgrade::run(handle.database());
            QVERIFY2(upgrade.ok, qPrintable(upgrade.errorMessage));
        }

        m_adminId = insertUser(QStringLiteral("jefa"), UserRole::Admin);
        m_sellerId = insertUser(QStringLiteral("vendedor"), UserRole::SalesAgent);
        QVERIFY(m_adminId > 0 && m_sellerId > 0);
    }

    void cleanupTestCase()
    {
        m_pool.reset();
        if (QSqlDatabase::contains(m_adminConnection)) {
            execOnDevelopment(QStringLiteral("DROP DATABASE IF EXISTS %1 WITH (FORCE)").arg(kScratchDatabase));
            QSqlDatabase::database(m_adminConnection).close();
            QSqlDatabase::removeDatabase(m_adminConnection);
        }
    }

    // Las dos columnas quedaron como en 00_schema.sql, y correrla otra vez no
    // cambia nada.
    void schemaUpgradeAddsTheColumnsOnce()
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        const SchemaUpgrade::Result again = SchemaUpgrade::run(handle.database());
        QVERIFY2(again.ok, qPrintable(again.errorMessage));
        QSqlQuery query(handle.database());
        QVERIFY(query.exec(QStringLiteral("SELECT column_name, column_default, is_nullable "
                                          "FROM information_schema.columns WHERE table_name = 'users' "
                                          "AND column_name IN ('active', 'last_login') ORDER BY 1")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("active"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("true"));
        QCOMPARE(query.value(2).toString(), QStringLiteral("NO"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("last_login"));
    }

    // El login no distingue mayúsculas y trae si la cuenta está activa.
    void loginLookupIgnoresCaseAndReportsActive()
    {
        const int id = insertUser(QStringLiteral("inactiva"), UserRole::SalesAgent, false);
        SqlUserDirectory directory(*m_pool);
        QString error;
        const auto record = directory.findByUsername(QStringLiteral("INACTIVA"), &error);
        QVERIFY2(record.has_value(), qPrintable(error));
        QCOMPARE(record->id, id);
        QCOMPARE(record->username, QStringLiteral("inactiva"));
        QVERIFY(!record->active);
    }

    // Una cuenta desactivada ya no se ofrece como vendedor.
    void deactivatedUsersAreNotOfferedAsVendors()
    {
        SqlSaleRepository sales(*m_pool);
        QString error;
        QStringList usernames;
        for (const auto &vendor : sales.searchVendors(QString(), &error))
            usernames << vendor.username;
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(usernames.contains(QStringLiteral("vendedor")));
        QVERIFY(!usernames.contains(QStringLiteral("inactiva")));
    }

    void createListsTheNewAccount()
    {
        SqlUserAccountRepository accounts(*m_pool);
        const Outcome created = accounts.createUser(
            {QStringLiteral("nueva"), QStringLiteral("Nueva Cuenta"), UserRole::SalesAgent,
             QStringLiteral("hash")},
            m_adminId);
        QCOMPARE(created.status, OutcomeStatus::Ok);
        const application::UserAccountDto user = account(created.id);
        QCOMPARE(user.username, QStringLiteral("nueva"));
        QCOMPARE(user.displayName, QStringLiteral("Nueva Cuenta"));
        QCOMPARE(user.role, UserRole::SalesAgent);
        QVERIFY(user.active);
        QVERIFY(!user.version.isEmpty());
        QVERIFY(user.createdAt.isValid());
    }

    // Sin importar mayúsculas: una cuenta vieja "Mayus" ya ocupa "mayus".
    void duplicateUsernameIsDetected()
    {
        insertUser(QStringLiteral("Mayus"), UserRole::SalesAgent);
        SqlUserAccountRepository accounts(*m_pool);
        const Outcome duplicate = accounts.createUser(
            {QStringLiteral("mayus"), QStringLiteral("Otra"), UserRole::SalesAgent, QStringLiteral("h")},
            m_adminId);
        QCOMPARE(duplicate.status, OutcomeStatus::DuplicateUsername);
    }

    void updateNeedsTheCurrentVersion()
    {
        SqlUserAccountRepository accounts(*m_pool);
        const application::UserAccountDto seller = account(m_sellerId);
        QCOMPARE(accounts.updateUser({m_sellerId, seller.version, QStringLiteral("Juan"), UserRole::SalesAgent},
                                     m_adminId)
                     .status,
                 OutcomeStatus::Ok);
        QCOMPARE(account(m_sellerId).displayName, QStringLiteral("Juan"));
        // Con la versión de antes, ya es un conflicto.
        QCOMPARE(accounts.updateUser({m_sellerId, seller.version, QStringLiteral("Pedro"), UserRole::SalesAgent},
                                     m_adminId)
                     .status,
                 OutcomeStatus::Conflict);
        QCOMPARE(accounts.updateUser({9999, seller.version, QStringLiteral("Nadie"), UserRole::SalesAgent},
                                     m_adminId)
                     .status,
                 OutcomeStatus::NotFound);
    }

    void deactivateAndResetPassword()
    {
        SqlUserAccountRepository accounts(*m_pool);
        const int id = insertUser(QStringLiteral("temporal"), UserRole::SalesAgent);
        QCOMPARE(accounts.setActive(id, account(id).version, false, m_adminId).status, OutcomeStatus::Ok);
        QVERIFY(!account(id).active);
        QCOMPARE(accounts.resetPassword(id, QStringLiteral("nuevo-hash"), m_adminId).status, OutcomeStatus::Ok);
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(QStringLiteral("SELECT password_hash FROM users WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), id);
        QVERIFY(query.exec() && query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("nuevo-hash"));
    }

    // Quien ya no es Administrador activo no administra nada.
    void actorMustStillBeAnActiveAdministrator()
    {
        SqlUserAccountRepository accounts(*m_pool);
        QCOMPARE(accounts.createUser({QStringLiteral("intento"), QStringLiteral("Intento"), UserRole::SalesAgent,
                                      QStringLiteral("h")},
                                     m_sellerId)
                     .status,
                 OutcomeStatus::ActorNotAllowed);
    }

    // La base nunca se queda sin un Administrador activo, aunque alguien
    // llamara al repositorio sin pasar por las reglas.
    void lastActiveAdministratorIsKept()
    {
        SqlUserAccountRepository accounts(*m_pool);
        QCOMPARE(accounts.setActive(m_adminId, account(m_adminId).version, false, m_adminId).status,
                 OutcomeStatus::LastActiveAdmin);
        QVERIFY(account(m_adminId).active);
        QCOMPARE(accounts.updateUser({m_adminId, account(m_adminId).version, QStringLiteral("Jefa"),
                                      UserRole::SalesAgent},
                                     m_adminId)
                     .status,
                 OutcomeStatus::LastActiveAdmin);
        QCOMPARE(account(m_adminId).role, UserRole::Admin);
    }

    // Dos administradores a la vez se forman: mientras otra sesión tiene el
    // candado de la administración de usuarios, una escritura espera.
    void writesWaitForAnotherAdministrator()
    {
        DatabaseConfig scratch = m_config;
        scratch.databaseName = kScratchDatabase;
        const QString connection = QStringLiteral("otra-sesion-usuarios");
        {
            QSqlDatabase other = QSqlDatabase::addDatabase(scratch.driverName, connection);
            other.setHostName(scratch.hostName);
            other.setPort(scratch.port);
            other.setDatabaseName(scratch.databaseName);
            other.setUserName(scratch.userName);
            other.setPassword(scratch.password);
            QVERIFY2(other.open(), qPrintable(other.lastError().text()));
            QVERIFY(other.transaction());
            QSqlQuery lock(other);
            QVERIFY(lock.exec(QStringLiteral("SELECT pg_advisory_xact_lock("
                                             "CAST('users'::regclass AS integer), 0)")));

            std::atomic<bool> finished{false};
            Outcome outcome;
            const QString version = account(m_sellerId).version;
            std::unique_ptr<QThread> worker(QThread::create([&] {
                SqlUserAccountRepository accounts(*m_pool);
                outcome = accounts.updateUser({m_sellerId, version, QStringLiteral("Mientras"),
                                               UserRole::SalesAgent},
                                              m_adminId);
                finished = true;
            }));
            worker->start();

            // Espera a que el hilo se quede esperando el candado. Se pregunta
            // desde otra conexión: dentro de una transacción, PostgreSQL
            // congela lo que devuelve pg_stat_activity.
            bool waited = false;
            const QDeadlineTimer deadline(10000);
            while (!deadline.hasExpired() && !finished) {
                {
                    ConnectionPool::Handle handle = m_pool->acquire();
                    QSqlQuery waiting(handle.database());
                    if (waiting.exec(QStringLiteral(
                            "SELECT 1 FROM pg_stat_activity WHERE datname = current_database() "
                            "AND wait_event_type = 'Lock' AND wait_event = 'advisory'"))
                        && waiting.next()) {
                        waited = true;
                        break;
                    }
                }
                QThread::msleep(20);
            }
            other.commit();
            worker->wait();
            other.close();
            QVERIFY2(waited, "la escritura no esperó a la otra sesión");
            QCOMPARE(outcome.status, OutcomeStatus::Ok);
        }
        QSqlDatabase::removeDatabase(connection);
    }
};

QTEST_GUILESS_MAIN(TstUsersDb)
#include "tst_users_db.moc"
