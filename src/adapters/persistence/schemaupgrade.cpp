#include "adapters/persistence/schemaupgrade.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QList>
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

} // namespace

namespace SchemaUpgrade
{

Result run(QSqlDatabase &db)
{
    Result result;

    // Columnas de users que el código usa y una base vieja puede no tener:
    // - last_login: la anota el login y la muestra Configuraciones. Llegó
    //   con el flujo de ventas, y las bases creadas antes no la tienen.
    // - active (US-07.1): las cuentas se desactivan en vez de borrarse.
    const QList<std::pair<QString, QString>> columns = {
        {QStringLiteral("last_login"), QStringLiteral("TIMESTAMP")},
        {QStringLiteral("active"), QStringLiteral("BOOLEAN NOT NULL DEFAULT TRUE")},
    };
    for (const auto &[column, definition] : columns) {
        QString error;
        if (hasColumn(db, QStringLiteral("users"), column, &error))
            continue;
        if (!error.isEmpty()) {
            result.errorMessage = error;
            return result;
        }
        QSqlQuery alter(db);
        if (!alter.exec(QStringLiteral("ALTER TABLE users ADD COLUMN IF NOT EXISTS %1 %2")
                            .arg(column, definition))) {
            result.errorMessage = alter.lastError().text();
            return result;
        }
    }

    result.ok = true;
    return result;
}

} // namespace SchemaUpgrade
