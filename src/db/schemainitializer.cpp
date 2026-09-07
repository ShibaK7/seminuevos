#include "../../include/db/schemainitializer.h"

#include <QFile>
#include <QIODevice>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

namespace {

// El script solo lo escribimos nosotros y sigue un formato regular (cada
// sentencia termina con ';' al final de su última línea, los comentarios
// van en líneas propias con '--'), así que un split simple por líneas es
// suficiente y evita depender del soporte multi-statement del driver.
QStringList splitStatements(const QString &script)
{
    QStringList statements;
    QString current;

    const QStringList lines = script.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString trimmedLine = rawLine.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith(QLatin1String("--")))
            continue;

        current += rawLine;
        current += QLatin1Char('\n');

        if (trimmedLine.endsWith(QLatin1Char(';'))) {
            statements << current.trimmed();
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty())
        statements << current.trimmed();

    return statements;
}

// UMA vigente usada por la regla de validación de pagos en efectivo (Paso 1
// del wizard de vehículos: Contado bloqueado si el precio >= 3210 * UMA). Se
// siembra aquí (no en DevSeeder) porque es dato de negocio, no de prueba, y
// debe existir también en producción. ON CONFLICT DO NOTHING la deja intacta
// si ya fue ajustada manualmente en la base.
bool seedUmaConfig(QSqlDatabase &db, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO global_configurations (key_param, value_param) "
        "VALUES ('UMA_DIARIA', 108.57) "
        "ON CONFLICT (key_param) DO NOTHING"));

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

} // namespace

SchemaInitializer::Result SchemaInitializer::run(QSqlDatabase &db)
{
    Result result;

    QFile file(QStringLiteral(":/db/001_init_schema.sql"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("No se pudo abrir el recurso :/db/001_init_schema.sql");
        return result;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString script = stream.readAll();
    file.close();

    const QStringList statements = splitStatements(script);
    if (statements.isEmpty()) {
        result.errorMessage = QStringLiteral("El script de esquema está vacío.");
        return result;
    }

    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1").arg(db.lastError().text());
        return result;
    }

    QSqlQuery query(db);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            result.errorMessage = QStringLiteral("Error ejecutando sentencia del esquema:\n%1\n\n%2")
                                       .arg(query.lastError().text(), statement);
            db.rollback();
            return result;
        }
    }

    QString seedError;
    if (!seedUmaConfig(db, seedError)) {
        result.errorMessage = QStringLiteral("Error sembrando configuración UMA_DIARIA: %1").arg(seedError);
        db.rollback();
        return result;
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1").arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    return result;
}
