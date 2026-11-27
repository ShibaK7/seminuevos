// Prueba de integración de los parámetros de Configuraciones (US-07.1, #83 y
// #84) contra el PostgreSQL de Docker, con SqlSettingsRepository y
// SqlReferenceDataReader. Es opcional: solo se compila con
// -DSEMINUEVOS_DB_TESTS=ON y necesita `docker compose up -d`.
//
// Trabaja en una base desechable (seminuevos_tst_settings) que crea con el
// 00_schema.sql del repositorio y borra al terminar: la app abierta contra la
// base de desarrollo nunca ve los valores de prueba, y si la prueba se
// interrumpe no se pierde la UMA capturada.

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/inventory/sqlreferencedatareader.h"
#include "adapters/persistence/settings/settingskeys.h"
#include "adapters/persistence/settings/sqlsettingsrepository.h"
#include "application/settings/services/systemsettingsservice.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QtTest>

#include <memory>

using domain::SystemParameter;
using Change = application::SettingsRepository::Change;
using UpdateStatus = application::SettingsRepository::UpdateOutcome::Status;

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

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

const QString kScratchDatabase = QStringLiteral("seminuevos_tst_settings");

// La fecha con que se siembran los renglones de cada caso: una que el
// guardado no puede dejar igual.
const QDateTime kSeededAt(QDate(2026, 1, 9), QTime(9, 0));

} // namespace

class TstSettingsDb : public QObject
{
    Q_OBJECT

private:
    // La conexión a la base de desarrollo, solo para crear y borrar la
    // desechable. Va con su propio nombre y no con otro ConnectionPool: dos
    // pools en el mismo hilo usarían el mismo nombre de conexión.
    const QString m_adminConnection = QStringLiteral("tst-settings-admin");
    // La desechable.
    std::unique_ptr<ConnectionPool> m_pool;
    // Quien guarda: un Administrador activo de la desechable.
    int m_adminId = -1;

    application::SessionDto admin() const
    {
        return {QStringLiteral("jefa"), QStringLiteral("Jefa"), domain::UserRole::Admin, m_adminId};
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

    static QStringList allKeys()
    {
        QStringList keys;
        for (const SystemParameter parameter : domain::allSystemParameters())
            keys << settingskeys::keyOf(parameter);
        return keys;
    }

    // Varias sentencias de una vez (sin prepare, que admite solo una).
    bool execScript(const QString &sql)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        if (!query.exec(sql)) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return false;
        }
        return true;
    }

    bool exec(const QString &sql, const QVariantMap &values = {})
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(sql);
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            query.bindValue(it.key(), it.value());
        if (!query.exec()) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return false;
        }
        return true;
    }

    // El valor guardado de una llave, como lo devuelve la base ("117.31"), o
    // vacío si no tiene renglón.
    QString valueOf(const QString &key)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(QStringLiteral(
            "SELECT CAST(value_param AS text) FROM global_configurations WHERE key_param = :key"));
        query.bindValue(QStringLiteral(":key"), key);
        return query.exec() && query.next() ? query.value(0).toString() : QString();
    }

    QDateTime updatedAtOf(const QString &key)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(QStringLiteral(
            "SELECT updated_at FROM global_configurations WHERE key_param = :key"));
        query.bindValue(QStringLiteral(":key"), key);
        return query.exec() && query.next() ? query.value(0).toDateTime() : QDateTime();
    }

    void seed(const QString &key, const QString &value)
    {
        QVERIFY(exec(QStringLiteral("INSERT INTO global_configurations (key_param, value_param, updated_at) "
                                    "VALUES (:key, CAST(:value AS NUMERIC), :at)"),
                     {{QStringLiteral(":key"), key},
                      {QStringLiteral(":value"), value},
                      {QStringLiteral(":at"), kSeededAt}}));
    }

    void clearKeys()
    {
        for (const QString &key : allKeys())
            QVERIFY(exec(QStringLiteral("DELETE FROM global_configurations WHERE key_param = :key"),
                         {{QStringLiteral(":key"), key}}));
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
        {
            QSqlDatabase development = QSqlDatabase::addDatabase(config.driverName, m_adminConnection);
            development.setHostName(config.hostName);
            development.setPort(config.port);
            development.setDatabaseName(config.databaseName);
            development.setUserName(config.userName);
            development.setPassword(config.password);
            if (!development.open())
                QSKIP("PostgreSQL no está disponible (docker compose up -d).");
        }
        execOnDevelopment(QStringLiteral("DROP DATABASE IF EXISTS %1 WITH (FORCE)").arg(kScratchDatabase));
        if (!execOnDevelopment(QStringLiteral("CREATE DATABASE %1").arg(kScratchDatabase)))
            QSKIP("El usuario de la base no puede crear una base desechable.");

        DatabaseConfig scratch = config;
        scratch.databaseName = kScratchDatabase;
        m_pool = std::make_unique<ConnectionPool>(scratch);
        m_pool->setGuiThreadAllowed(true);
        const QString schema =
            readFile(QStringLiteral(SEMINUEVOS_SOURCE_DIR) + QStringLiteral("/init-db/00_schema.sql"));
        QVERIFY(!schema.isEmpty());
        QVERIFY(execScript(schema));
        QVERIFY(execScript(QStringLiteral(
            "INSERT INTO users (username, password_hash, display_name, role) "
            "VALUES ('jefa', 'x', 'Jefa', 'Administrador')")));
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        QVERIFY(query.exec(QStringLiteral("SELECT id FROM users WHERE username = 'jefa'")) && query.next());
        m_adminId = query.value(0).toInt();
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

    // Cada caso parte de la UMA y el tope capturados.
    void init()
    {
        clearKeys();
        seed(settingskeys::umaDailyValue(), QStringLiteral("117.31"));
        seed(settingskeys::cashLimitUmaMultiple(), QStringLiteral("3210"));
    }

    // #83: los que tienen renglón, en centésimos y con su fecha.
    void readsStoredParametersWithTheirDates()
    {
        SqlSettingsRepository repository(*m_pool);
        QString error;
        const QList<application::SystemParameterDto> rows = repository.getSystemParameters(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(rows.size(), 2);
        for (const application::SystemParameterDto &row : rows) {
            QCOMPARE(row.updatedAt, kSeededAt);
            if (row.parameter == SystemParameter::UmaDailyValue)
                QCOMPARE(row.hundredths, std::optional<qint64>(11731));
            if (row.parameter == SystemParameter::CashLimitUmaMultiple)
                QCOMPARE(row.hundredths, std::optional<qint64>(321000));
        }
    }

    // #84: solo lo que cambió recibe fecha nueva.
    void onlyChangedParametersGetANewDate()
    {
        SqlSettingsRepository repository(*m_pool);
        const auto outcome = repository.updateSystemParameters(
            {Change{SystemParameter::UmaDailyValue, 11731, 12000}}, m_adminId);
        QCOMPARE(outcome.status, UpdateStatus::Updated);
        QCOMPARE(valueOf(settingskeys::umaDailyValue()), QStringLiteral("120.00"));
        QVERIFY(updatedAtOf(settingskeys::umaDailyValue()) > kSeededAt);
        QCOMPARE(updatedAtOf(settingskeys::cashLimitUmaMultiple()), kSeededAt);
    }

    // Un parámetro que nunca se capturó se crea al guardarlo.
    void missingParameterIsCreated()
    {
        QVERIFY(exec(QStringLiteral("DELETE FROM global_configurations WHERE key_param = :key"),
                     {{QStringLiteral(":key"), settingskeys::cashLimitUmaMultiple()}}));
        SqlSettingsRepository repository(*m_pool);
        const auto outcome = repository.updateSystemParameters(
            {Change{SystemParameter::CashLimitUmaMultiple, std::nullopt, 200000}}, m_adminId);
        QCOMPARE(outcome.status, UpdateStatus::Updated);
        QCOMPARE(valueOf(settingskeys::cashLimitUmaMultiple()), QStringLiteral("2000.00"));
        QVERIFY(updatedAtOf(settingskeys::cashLimitUmaMultiple()).isValid());
    }

    // Otro administrador cambió la UMA desde que se leyó: no se pisa, y
    // tampoco se guarda lo demás (todo o nada).
    void staleValueIsAConflictAndNothingIsSaved()
    {
        QVERIFY(exec(QStringLiteral("UPDATE global_configurations SET value_param = 118 "
                                    "WHERE key_param = :key"),
                     {{QStringLiteral(":key"), settingskeys::umaDailyValue()}}));
        SqlSettingsRepository repository(*m_pool);
        const auto outcome = repository.updateSystemParameters(
            {Change{SystemParameter::CashLimitUmaMultiple, 321000, 200000},
             Change{SystemParameter::UmaDailyValue, 11731, 12000}}, m_adminId);
        QCOMPARE(outcome.status, UpdateStatus::Conflict);
        QCOMPARE(valueOf(settingskeys::cashLimitUmaMultiple()), QStringLiteral("3210.00"));
        QCOMPARE(valueOf(settingskeys::umaDailyValue()), QStringLiteral("118.00"));
        QCOMPARE(updatedAtOf(settingskeys::cashLimitUmaMultiple()), kSeededAt);
    }

    // Otro administrador lo creó mientras tanto: tampoco se pisa.
    void createdMeanwhileIsAConflict()
    {
        QVERIFY(exec(QStringLiteral("DELETE FROM global_configurations WHERE key_param = :key"),
                     {{QStringLiteral(":key"), settingskeys::cashLimitUmaMultiple()}}));
        seed(settingskeys::cashLimitUmaMultiple(), QStringLiteral("1000"));
        SqlSettingsRepository repository(*m_pool);
        const auto outcome = repository.updateSystemParameters(
            {Change{SystemParameter::CashLimitUmaMultiple, std::nullopt, 200000}}, m_adminId);
        QCOMPARE(outcome.status, UpdateStatus::Conflict);
        QCOMPARE(valueOf(settingskeys::cashLimitUmaMultiple()), QStringLiteral("1000.00"));
    }

    // La sesión se abrió como Administrador, pero ya le quitaron el rol (o
    // la desactivaron): no se guarda nada.
    void actorWhoIsNoLongerAnAdministratorSavesNothing()
    {
        SqlSettingsRepository repository(*m_pool);
        const QList<Change> change{Change{SystemParameter::CashLimitUmaMultiple, 321000, 200000}};
        QVERIFY(exec(QStringLiteral("UPDATE users SET role = 'Vendedor' WHERE id = :id"),
                     {{QStringLiteral(":id"), m_adminId}}));
        QCOMPARE(repository.updateSystemParameters(change, m_adminId).status, UpdateStatus::ActorNotAllowed);
        QVERIFY(exec(QStringLiteral("UPDATE users SET role = 'Administrador', active = FALSE WHERE id = :id"),
                     {{QStringLiteral(":id"), m_adminId}}));
        QCOMPARE(repository.updateSystemParameters(change, m_adminId).status, UpdateStatus::ActorNotAllowed);
        QVERIFY(exec(QStringLiteral("UPDATE users SET active = TRUE WHERE id = :id"),
                     {{QStringLiteral(":id"), m_adminId}}));
        QCOMPARE(valueOf(settingskeys::cashLimitUmaMultiple()), QStringLiteral("3210.00"));
        QCOMPARE(updatedAtOf(settingskeys::cashLimitUmaMultiple()), kSeededAt);
    }

    // De la pantalla a la base y de vuelta, con el servicio.
    void serviceRoundTrip()
    {
        SqlSettingsRepository repository(*m_pool);
        const application::SystemSettingsService service(repository);
        const auto loaded = service.getSystemParameters(admin());
        QCOMPARE(loaded.status, application::SystemParametersResult::Status::Loaded);
        QList<application::SystemParameterEditDto> edits;
        for (const application::SystemParameterDto &dto : loaded.parameters) {
            std::optional<qint64> shown =
                dto.hundredths ? dto.hundredths : domain::defaultHundredths(dto.parameter);
            if (dto.parameter == SystemParameter::CashLimitUmaMultiple)
                shown = 200000; // 2,000 UMA
            edits << application::SystemParameterEditDto{dto.parameter, dto.hundredths, shown};
        }
        const auto saved = service.updateSystemParameters(edits, admin());
        QCOMPARE(saved.status, application::SystemParametersUpdateResult::Status::Saved);
        QCOMPARE(saved.updated, QList<SystemParameter>{SystemParameter::CashLimitUmaMultiple});
        QCOMPARE(valueOf(settingskeys::cashLimitUmaMultiple()), QStringLiteral("2000.00"));
    }

    // El tope que se guarda es el que lee la regla del efectivo; uno inválido
    // o ausente se toma como el legal.
    void cashLimitTermsFollowTheConfiguredMultiple()
    {
        SqlReferenceDataReader reference(*m_pool);
        QString error;
        QVERIFY(exec(QStringLiteral("UPDATE global_configurations SET value_param = 1000 WHERE key_param = :key"),
                     {{QStringLiteral(":key"), settingskeys::cashLimitUmaMultiple()}}));
        domain::CashLimitTerms terms = reference.cashLimitTerms(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(terms.umaDailyValue, 117.31);
        QCOMPARE(terms.umaMultiple, 1000);

        QVERIFY(exec(QStringLiteral("UPDATE global_configurations SET value_param = 5000 WHERE key_param = :key"),
                     {{QStringLiteral(":key"), settingskeys::cashLimitUmaMultiple()}}));
        QCOMPARE(reference.cashLimitTerms(&error).umaMultiple, 3210);

        QVERIFY(exec(QStringLiteral("DELETE FROM global_configurations WHERE key_param = :key"),
                     {{QStringLiteral(":key"), settingskeys::cashLimitUmaMultiple()}}));
        QCOMPARE(reference.cashLimitTerms(&error).umaMultiple, 3210);
    }
};

QTEST_GUILESS_MAIN(TstSettingsDb)
#include "tst_settings_db.moc"
