// Prueba de integración de las cuentas de dinero (Caja Chica, Cantidad Banco
// y Caja Fuerte) contra el PostgreSQL de Docker. Es opcional: solo se compila
// con -DSEMINUEVOS_DB_TESTS=ON y necesita `docker compose up -d`.
//
// Trabaja en una base desechable (seminuevos_tst_cash) que crea con el
// 00_schema.sql del repositorio y borra al terminar.

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/finance/cashledgerwriter.h"
#include "adapters/persistence/finance/sqlcashledger.h"
#include "adapters/persistence/schemaupgrade.h"

#include <QDeadlineTimer>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QThread>
#include <QtTest>

#include <atomic>
#include <memory>

using domain::CashAccount;
using WriteStatus = application::CashLedger::WriteOutcome::Status;

namespace {

const QString kScratchDatabase = QStringLiteral("seminuevos_tst_cash");

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

class TstCashDb : public QObject
{
    Q_OBJECT

private:
    DatabaseConfig m_config;
    // La conexión a la base de desarrollo, solo para crear y borrar la
    // desechable (con su propio nombre: dos pools en un hilo chocarían).
    const QString m_adminConnection = QStringLiteral("tst-cash-admin");
    std::unique_ptr<ConnectionPool> m_pool;
    int m_adminId = -1;
    int m_sellerId = -1;

    bool exec(const QString &sql)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
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

    qint64 scalar(const QString &sql)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        if (!query.exec(sql) || !query.next()) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return -1;
        }
        return query.value(0).toLongLong();
    }

    qint64 balance(CashAccount account)
    {
        SqlCashLedger ledger(*m_pool);
        QString error;
        for (const application::CashBalanceDto &dto : ledger.balances(&error)) {
            if (dto.account == account)
                return dto.hundredths;
        }
        return -1;
    }

    int insertUser(const QString &username, const QString &role)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(QStringLiteral("INSERT INTO users (username, password_hash, display_name, role) "
                                     "VALUES (:username, 'x', :name, :role) RETURNING id"));
        query.bindValue(QStringLiteral(":username"), username);
        query.bindValue(QStringLiteral(":name"), username.toUpper());
        query.bindValue(QStringLiteral(":role"), role);
        if (!query.exec() || !query.next()) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return -1;
        }
        return query.value(0).toInt();
    }

    // Abre Caja Chica y Banco con estos saldos (Caja Fuerte en cero).
    void openWith(qint64 pettyCash, qint64 bank)
    {
        SqlCashLedger ledger(*m_pool);
        QCOMPARE(ledger.open({pettyCash, bank}, m_adminId).status, WriteStatus::Ok);
    }

    QSqlDatabase otherSession(const QString &name)
    {
        QSqlDatabase other = QSqlDatabase::addDatabase(m_config.driverName, name);
        other.setHostName(m_config.hostName);
        other.setPort(m_config.port);
        other.setDatabaseName(kScratchDatabase);
        other.setUserName(m_config.userName);
        other.setPassword(m_config.password);
        return other;
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
        const QString schema =
            readFile(QStringLiteral(SEMINUEVOS_SOURCE_DIR) + QStringLiteral("/init-db/00_schema.sql"));
        QVERIFY(!schema.isEmpty());
        QVERIFY(exec(schema));
        m_adminId = insertUser(QStringLiteral("jefa"), QStringLiteral("Administrador"));
        m_sellerId = insertUser(QStringLiteral("vendedor"), QStringLiteral("Vendedor"));
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

    // Cada caso parte de las cuentas recién creadas: sin abrir, en cero.
    void init()
    {
        QVERIFY(exec(QStringLiteral("DELETE FROM cash_movements")));
        QVERIFY(exec(QStringLiteral("UPDATE cash_accounts SET balance = 0, updated_at = NULL, "
                                    "opened = (account = 'Caja Fuerte')")));
    }

    // En una base de antes, SchemaUpgrade crea las dos tablas con sus tres
    // cuentas; correrla otra vez no cambia nada.
    void schemaUpgradeCreatesTheLedger()
    {
        QVERIFY(exec(QStringLiteral("DROP TABLE cash_movements; DROP TABLE cash_accounts; "
                                    "DROP SEQUENCE cash_transfer_seq")));
        for (int run = 0; run < 2; ++run) {
            ConnectionPool::Handle handle = m_pool->acquire();
            const SchemaUpgrade::Result upgrade = SchemaUpgrade::run(handle.database());
            QVERIFY2(upgrade.ok, qPrintable(upgrade.errorMessage));
        }
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_accounts")), qint64(3));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_accounts WHERE opened")), qint64(1));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_movements")), qint64(0));
    }

    // Dos apps que arrancan a la vez sobre una base vieja: las dos abren, y
    // las tablas quedan una sola vez.
    void simultaneousUpgradesBothSucceed()
    {
        for (int round = 0; round < 3; ++round) {
            QVERIFY(exec(QStringLiteral("DROP TABLE cash_movements; DROP TABLE cash_accounts; "
                                        "DROP SEQUENCE cash_transfer_seq")));
            SchemaUpgrade::Result results[2];
            std::unique_ptr<QThread> workers[2];
            for (int i = 0; i < 2; ++i) {
                workers[i].reset(QThread::create([this, &results, i] {
                    ConnectionPool::Handle handle = m_pool->acquire();
                    results[i] = SchemaUpgrade::run(handle.database());
                }));
            }
            for (auto &worker : workers)
                worker->start();
            for (auto &worker : workers)
                worker->wait();
            for (const SchemaUpgrade::Result &result : results)
                QVERIFY2(result.ok, qPrintable(result.errorMessage));
            QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_accounts")), qint64(3));
        }
    }

    // Una sola vez: la segunda no suma nada.
    void openingHappensOnce()
    {
        openWith(2000000, 10000000);
        QCOMPARE(balance(CashAccount::PettyCash), qint64(2000000));
        QCOMPARE(balance(CashAccount::Bank), qint64(10000000));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_movements WHERE kind = 'Apertura'")),
                 qint64(2));

        SqlCashLedger ledger(*m_pool);
        QCOMPARE(ledger.open({1, 1}, m_adminId).status, WriteStatus::AlreadyOpened);
        QCOMPARE(balance(CashAccount::PettyCash), qint64(2000000));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_movements")), qint64(2));
    }

    // Una venta entró antes de abrir la cuenta: lo capturado ya la incluye
    // (es lo que hay contado), así que el saldo queda en lo capturado y la
    // apertura registra solo la diferencia.
    void openingSetsTheCountedBalance()
    {
        {
            ConnectionPool::Handle handle = m_pool->acquire();
            QSqlDatabase &db = handle.database();
            QVERIFY(db.transaction());
            const auto sale = cashledger::applyMovements(
                db, {domain::CashMovement::inflow(CashAccount::PettyCash, 5000000, domain::CashMovementKind::Sale,
                                                  QStringLiteral("Venta"), QDate::currentDate()),
                     domain::CashMovement::inflow(CashAccount::Bank, 100000, domain::CashMovementKind::Sale,
                                                  QStringLiteral("Venta"), QDate::currentDate())});
            QCOMPARE(sale.status, cashledger::ApplyOutcome::Status::Applied);
            QVERIFY(db.commit());
        }
        openWith(2000000, 7000000);
        QCOMPARE(balance(CashAccount::PettyCash), qint64(2000000));
        QCOMPARE(balance(CashAccount::Bank), qint64(7000000));
        QCOMPARE(scalar(QStringLiteral("SELECT CAST(SUM(amount) * 100 AS BIGINT) FROM cash_movements "
                                       "WHERE account = 'Caja Chica' AND kind = 'Apertura'")),
                 qint64(-3000000));
        QCOMPARE(scalar(QStringLiteral("SELECT CAST(SUM(amount) * 100 AS BIGINT) FROM cash_movements "
                                       "WHERE account = 'Banco' AND kind = 'Apertura'")),
                 qint64(6900000));
    }

    // Un traspaso: sale del origen, entra a Caja Fuerte, las dos patas unidas.
    void depositIsATransfer()
    {
        openWith(500000, 0);
        SqlCashLedger ledger(*m_pool);
        QCOMPARE(ledger.depositToSafe({CashAccount::PettyCash, 200000}, m_adminId).status, WriteStatus::Ok);
        QCOMPARE(balance(CashAccount::PettyCash), qint64(300000));
        QCOMPARE(balance(CashAccount::Safe), qint64(200000));
        QCOMPARE(scalar(QStringLiteral("SELECT count(DISTINCT transfer_id) FROM cash_movements "
                                       "WHERE kind = 'Traspaso'")),
                 qint64(1));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_movements WHERE kind = 'Traspaso' "
                                       "AND user_id IS NOT NULL")),
                 qint64(2));
        QCOMPARE(scalar(QStringLiteral("SELECT CAST(SUM(amount) * 100 AS BIGINT) FROM cash_movements "
                                       "WHERE kind = 'Traspaso'")),
                 qint64(0));

        const auto movements = ledger.recentMovements(CashAccount::Safe, 10, nullptr);
        QCOMPARE(movements.size(), 1);
        QCOMPARE(movements.first().hundredths, qint64(200000));
        QCOMPARE(movements.first().userName, QStringLiteral("JEFA"));
    }

    // Sin saldo no se aplica nada: ni la salida ni la entrada.
    void insufficientDepositChangesNothing()
    {
        openWith(100000, 0);
        SqlCashLedger ledger(*m_pool);
        const auto outcome = ledger.depositToSafe({CashAccount::PettyCash, 100001}, m_adminId);
        QCOMPARE(outcome.status, WriteStatus::Insufficient);
        QCOMPARE(outcome.account, CashAccount::PettyCash);
        QCOMPARE(outcome.available, qint64(100000));
        QCOMPARE(balance(CashAccount::PettyCash), qint64(100000));
        QCOMPARE(balance(CashAccount::Safe), qint64(0));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_movements WHERE kind = 'Traspaso'")),
                 qint64(0));
    }

    void withdrawalKeepsWhenAndWhat()
    {
        openWith(500000, 0);
        SqlCashLedger ledger(*m_pool);
        QCOMPARE(ledger.depositToSafe({CashAccount::PettyCash, 300000}, m_adminId).status, WriteStatus::Ok);
        const QDate when(2026, 11, 20);
        QCOMPARE(ledger.withdrawFromSafe({when, QStringLiteral("Pago de nómina"), 100000}, m_adminId).status,
                 WriteStatus::Ok);
        QCOMPARE(balance(CashAccount::Safe), qint64(200000));
        const auto movements = ledger.recentMovements(CashAccount::Safe, 1, nullptr);
        QCOMPARE(movements.first().kind, domain::CashMovementKind::Withdrawal);
        QCOMPARE(movements.first().concept, QStringLiteral("Pago de nómina"));
        QCOMPARE(movements.first().date, when);
        QCOMPARE(movements.first().hundredths, qint64(-100000));
    }

    // Quien ya no es Administrador activo no mueve nada.
    void onlyAnActiveAdministratorWrites()
    {
        SqlCashLedger ledger(*m_pool);
        QCOMPARE(ledger.open({100, 100}, m_sellerId).status, WriteStatus::ActorNotAllowed);
        QCOMPARE(balance(CashAccount::PettyCash), qint64(0));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_accounts WHERE opened")), qint64(1));
    }

    // El ayudante corre dentro de la transacción de quien llama: si esa se
    // deshace, el movimiento también.
    void writerFollowsTheCallersTransaction()
    {
        openWith(100000, 0);
        {
            ConnectionPool::Handle handle = m_pool->acquire();
            QSqlDatabase &db = handle.database();
            QVERIFY(db.transaction());
            const auto applied = cashledger::applyMovements(
                db, {domain::CashMovement::outflow(CashAccount::PettyCash, 40000, domain::CashMovementKind::Purchase,
                                                   QStringLiteral("Compra"), QDate::currentDate())});
            QCOMPARE(applied.status, cashledger::ApplyOutcome::Status::Applied);
            QVERIFY(db.rollback());
        }
        QCOMPARE(balance(CashAccount::PettyCash), qint64(100000));
        QCOMPARE(scalar(QStringLiteral("SELECT count(*) FROM cash_movements WHERE kind = 'Compra'")),
                 qint64(0));
    }

    // Dos sesiones sobre la misma cuenta: la segunda espera a la primera y
    // decide con el saldo que dejó, así que nunca queda en negativo.
    void concurrentOutflowsNeverOverdraw()
    {
        openWith(100000, 0);
        const QString connection = QStringLiteral("otra-sesion-caja");
        {
            QSqlDatabase other = otherSession(connection);
            QVERIFY2(other.open(), qPrintable(other.lastError().text()));
            QVERIFY(other.transaction());
            const auto first = cashledger::applyMovements(
                other, {domain::CashMovement::outflow(CashAccount::PettyCash, 70000,
                                                      domain::CashMovementKind::Purchase,
                                                      QStringLiteral("Primera"), QDate::currentDate())});
            QCOMPARE(first.status, cashledger::ApplyOutcome::Status::Applied);

            std::atomic<bool> finished{false};
            application::CashLedger::WriteOutcome outcome;
            std::unique_ptr<QThread> worker(QThread::create([&] {
                SqlCashLedger ledger(*m_pool);
                outcome = ledger.depositToSafe({CashAccount::PettyCash, 50000}, m_adminId);
                finished = true;
            }));
            worker->start();

            bool waited = false;
            const QDeadlineTimer deadline(10000);
            while (!deadline.hasExpired() && !finished) {
                if (scalar(QStringLiteral("SELECT count(*) FROM pg_stat_activity WHERE datname = "
                                          "current_database() AND wait_event_type = 'Lock'"))
                    > 0) {
                    waited = true;
                    break;
                }
                QThread::msleep(20);
            }
            QVERIFY(other.commit());
            worker->wait();
            other.close();
            QVERIFY2(waited, "el traspaso no esperó a la otra sesión");
            QCOMPARE(outcome.status, WriteStatus::Insufficient);
            QCOMPARE(outcome.available, qint64(30000));
        }
        QSqlDatabase::removeDatabase(connection);
        QCOMPARE(balance(CashAccount::PettyCash), qint64(30000));
        QCOMPARE(balance(CashAccount::Safe), qint64(0));
    }
};

QTEST_GUILESS_MAIN(TstCashDb)
#include "tst_cash_db.moc"
