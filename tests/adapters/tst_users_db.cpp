// Prueba de integración de las cuentas de usuario (US-07.1) contra el
// PostgreSQL de Docker. Es opcional: solo se compila con
// -DSEMINUEVOS_DB_TESTS=ON y necesita `docker compose up -d`.
//
// Crea cuentas con un usuario único que empieza con "tstu" y las borra al
// terminar. Ninguna tiene ventas, así que se pueden borrar.

#include "adapters/persistence/auth/sqluserdirectory.h"
#include "adapters/persistence/commercial/sqlsalerepository.h"
#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/schemaupgrade.h"

#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QtTest>

#include <memory>

namespace {

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

} // namespace

class TstUsersDb : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<ConnectionPool> m_pool;
    QStringList m_created;

    // Un usuario que no choca con otra corrida ni con otro caso.
    QString uniqueUsername()
    {
        static int sequence = 0;
        const QString username =
            QStringLiteral("tstu%1%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(++sequence);
        m_created << username;
        return username;
    }

    // Inserta una cuenta directamente, como lo hacía DevSeeder.
    int insertUser(const QString &username, const QString &displayName, const QString &role,
                   bool active = true)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(QStringLiteral(
            "INSERT INTO users (username, password_hash, display_name, role, active) "
            "VALUES (:username, 'x', :name, :role, :active) RETURNING id"));
        query.bindValue(QStringLiteral(":username"), username);
        query.bindValue(QStringLiteral(":name"), displayName);
        query.bindValue(QStringLiteral(":role"), role);
        query.bindValue(QStringLiteral(":active"), active);
        if (!query.exec() || !query.next()) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return -1;
        }
        return query.value(0).toInt();
    }

private slots:
    void initTestCase()
    {
        const QMap<QString, QString> env = readEnv();
        DatabaseConfig config;
        config.hostName = env.value(QStringLiteral("POSTGRES_HOST"), QStringLiteral("localhost"));
        config.port = env.value(QStringLiteral("POSTGRES_PORT"), QStringLiteral("5432")).toInt();
        config.databaseName = env.value(QStringLiteral("POSTGRES_DB"), QStringLiteral("seminuevos"));
        config.userName = env.value(QStringLiteral("POSTGRES_USER"), QStringLiteral("seminuevos_app"));
        config.password = env.value(QStringLiteral("POSTGRES_PASSWORD"));
        m_pool = std::make_unique<ConnectionPool>(config);
        m_pool->setGuiThreadAllowed(true);
        ConnectionPool::Handle handle = m_pool->acquire();
        if (!handle.database().isOpen())
            QSKIP("PostgreSQL no está disponible (docker compose up -d).");
        // Lo mismo que hace la app al arrancar: en una base anterior a la
        // US-07.1 agrega users.active.
        const SchemaUpgrade::Result upgrade = SchemaUpgrade::run(handle.database());
        QVERIFY2(upgrade.ok, qPrintable(upgrade.errorMessage));
    }

    void cleanupTestCase()
    {
        if (!m_pool)
            return;
        ConnectionPool::Handle handle = m_pool->acquire();
        for (const QString &username : std::as_const(m_created)) {
            QSqlQuery query(handle.database());
            query.prepare(QStringLiteral("DELETE FROM users WHERE lower(username) = lower(:username)"));
            query.bindValue(QStringLiteral(":username"), username);
            query.exec();
        }
    }

    // Correrla otra vez en una base al día no cambia nada ni falla.
    void schemaUpgradeIsIdempotent()
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        const SchemaUpgrade::Result again = SchemaUpgrade::run(handle.database());
        QVERIFY2(again.ok, qPrintable(again.errorMessage));
        QSqlQuery query(handle.database());
        QVERIFY(query.exec(QStringLiteral("SELECT column_default, is_nullable FROM information_schema.columns "
                                          "WHERE table_name = 'users' AND column_name = 'active'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("true"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("NO"));
    }

    // El login no distingue mayúsculas y trae si la cuenta está activa.
    void loginLookupIgnoresCaseAndReportsActive()
    {
        const QString username = uniqueUsername();
        const int id = insertUser(username, QStringLiteral("Prueba Login"), QStringLiteral("Vendedor"), false);
        QVERIFY(id > 0);
        SqlUserDirectory directory(*m_pool);
        QString error;
        const auto record = directory.findByUsername(username.toUpper(), &error);
        QVERIFY2(record.has_value(), qPrintable(error));
        QCOMPARE(record->id, id);
        QCOMPARE(record->username, username);
        QVERIFY(!record->active);
    }

    // Una cuenta desactivada ya no se ofrece como vendedor.
    void deactivatedUsersAreNotOfferedAsVendors()
    {
        const QString active = uniqueUsername();
        const QString inactive = uniqueUsername();
        QVERIFY(insertUser(active, QStringLiteral("Vendedor Activo Tstu"), QStringLiteral("Vendedor")) > 0);
        QVERIFY(insertUser(inactive, QStringLiteral("Vendedor Inactivo Tstu"), QStringLiteral("Vendedor"), false) > 0);
        SqlSaleRepository sales(*m_pool);
        QString error;
        const auto vendors = sales.searchVendors(QStringLiteral("tstu"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QStringList usernames;
        for (const auto &vendor : vendors)
            usernames << vendor.username;
        QVERIFY2(usernames.contains(active), qPrintable(usernames.join(QStringLiteral(", "))));
        QVERIFY(!usernames.contains(inactive));
    }
};

QTEST_GUILESS_MAIN(TstUsersDb)
#include "tst_users_db.moc"
