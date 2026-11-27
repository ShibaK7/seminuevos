#include "adapters/persistence/schemaupgrade.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QList>
#include <QStringList>
#include <QVariant>

#include <utility>

namespace {

// Si la tabla ya tiene la columna. Se pregunta antes de alterar: un ALTER
// TABLE, aunque lleve IF NOT EXISTS, pide un candado exclusivo sobre la tabla.
bool hasColumn(QSqlDatabase &db, const QString &table, const QString &column, QString *error)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM information_schema.columns "
                                 "WHERE table_schema = current_schema() "
                                 "AND table_name = :table AND column_name = :column"));
    query.bindValue(QStringLiteral(":table"), table);
    query.bindValue(QStringLiteral(":column"), column);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    return query.next();
}

bool hasTable(QSqlDatabase &db, const QString &table, QString *error)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM information_schema.tables "
                                 "WHERE table_schema = current_schema() AND table_name = :table"));
    query.bindValue(QStringLiteral(":table"), table);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    return query.next();
}

// Las cuentas de dinero y su bitácora (Configuraciones), igual que en
// 00_schema.sql. Todo en una transacción: o quedan las dos tablas con sus
// tres cuentas, o ninguna.
bool createCashLedger(QSqlDatabase &db, QString *error)
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cash_accounts ("
            " account VARCHAR(20) PRIMARY KEY CHECK (account IN ('Caja Chica', 'Banco', 'Caja Fuerte')),"
            " balance NUMERIC(14, 2) NOT NULL DEFAULT 0 CHECK (balance >= 0),"
            " opened BOOLEAN NOT NULL DEFAULT FALSE,"
            " updated_at TIMESTAMP)"),
        QStringLiteral("INSERT INTO cash_accounts (account, opened) VALUES "
                       "('Caja Chica', FALSE), ('Banco', FALSE), ('Caja Fuerte', TRUE) "
                       "ON CONFLICT (account) DO NOTHING"),
        QStringLiteral("CREATE SEQUENCE IF NOT EXISTS cash_transfer_seq"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cash_movements ("
            " id SERIAL PRIMARY KEY,"
            " account VARCHAR(20) NOT NULL REFERENCES cash_accounts(account),"
            " amount NUMERIC(12, 2) NOT NULL CHECK (amount <> 0),"
            " kind VARCHAR(30) NOT NULL CHECK (kind IN ('Apertura', 'Compra', 'Corrección de compra',"
            "   'Venta', 'Enganche', 'Traspaso', 'Retiro', 'Mantenimiento', 'Gasto')),"
            " concept TEXT NOT NULL,"
            " occurred_on DATE NOT NULL DEFAULT CURRENT_DATE,"
            " created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            " user_id INTEGER REFERENCES users(id),"
            " transfer_id INTEGER,"
            " vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE SET NULL,"
            " sale_folio INTEGER REFERENCES sales(folio) ON DELETE SET NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cash_movements_account "
                       "ON cash_movements (account, created_at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cash_movements_vehicle ON cash_movements (vehicle_folio)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cash_movements_sale ON cash_movements (sale_folio)"),
    };
    if (!db.transaction()) {
        *error = db.lastError().text();
        return false;
    }
    // Dos apps que arrancan a la vez sobre una base vieja: la segunda espera
    // a la primera y, si ya las creó, no hace nada. Sin esto, su CREATE TABLE
    // IF NOT EXISTS choca con el de la otra (pg_type) y la app no abre.
    QSqlQuery lock(db);
    if (!lock.exec(QStringLiteral("SELECT pg_advisory_xact_lock(hashtext('seminuevos.schemaupgrade'))"))) {
        *error = lock.lastError().text();
        db.rollback();
        return false;
    }
    if (hasTable(db, QStringLiteral("cash_movements"), error)) {
        db.rollback();
        return true;
    }
    if (!error->isEmpty()) {
        db.rollback();
        return false;
    }
    for (const QString &statement : statements) {
        QSqlQuery query(db);
        if (!query.exec(statement)) {
            *error = query.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        *error = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

} // namespace

namespace SchemaUpgrade
{

Result run(QSqlDatabase &db)
{
    Result result;

    // Columnas que el código usa y una base vieja puede no tener:
    // - users.last_login: la anota el login y la muestra Configuraciones.
    //   Llegó con el flujo de ventas, y las bases creadas antes no la tienen.
    // - users.active (US-07.1): las cuentas se desactivan en vez de borrarse.
    // - sale_financing.down_payment_method: cómo se pagó el enganche, que
    //   decide si entra a Caja Chica o a Banco.
    struct Column
    {
        QString table;
        QString name;
        QString definition;
    };
    const QList<Column> columns = {
        {QStringLiteral("users"), QStringLiteral("last_login"), QStringLiteral("TIMESTAMP")},
        {QStringLiteral("users"), QStringLiteral("active"), QStringLiteral("BOOLEAN NOT NULL DEFAULT TRUE")},
        {QStringLiteral("sale_financing"), QStringLiteral("down_payment_method"),
         QStringLiteral("VARCHAR(30) CHECK (down_payment_method IN ('Efectivo', 'Transferencia'))")},
    };
    for (const Column &column : columns) {
        QString error;
        if (hasColumn(db, column.table, column.name, &error))
            continue;
        if (!error.isEmpty()) {
            result.errorMessage = error;
            return result;
        }
        QSqlQuery alter(db);
        if (!alter.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN IF NOT EXISTS %2 %3")
                            .arg(column.table, column.name, column.definition))) {
            result.errorMessage = alter.lastError().text();
            return result;
        }
    }

    // Las cuentas de dinero y sus movimientos (Configuraciones). Se crean
    // juntas, así que basta preguntar por la bitácora.
    QString error;
    if (!hasTable(db, QStringLiteral("cash_movements"), &error)) {
        if (!error.isEmpty() || !createCashLedger(db, &error)) {
            result.errorMessage = error;
            return result;
        }
    }

    result.ok = true;
    return result;
}

} // namespace SchemaUpgrade
