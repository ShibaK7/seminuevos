#include "../../include/db/schemainitializer.h"

#include <QFile>
#include <QIODevice>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>
#include <QVariant>

namespace {

// El script solo lo escribimos nosotros y sigue un formato regular (cada
// sentencia termina con ';' al final de su última línea, los comentarios
// van en líneas propias con '--'), así que un split simple por líneas es
// suficiente y evita depender del soporte multi-statement del driver.
// Ejecutar sentencia por sentencia también nos da el mensaje de error
// atribuido a la sentencia exacta que falló, que es todo el valor de esto.
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

// Fila única a nivel de base: 'singleton' solo admite TRUE (por el CHECK) y
// es la llave primaria, así que la tabla no puede tener más de un renglón.
// La crea el C++ y no el .sql a propósito: si el script la dropeara, la app
// perdería la cuenta de qué versión tiene instalada y volvería a resetear en
// cada arranque, en silencio.
bool ensureVersionTable(QSqlDatabase &db, QString &errorMessage)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_version ("
            "  singleton BOOLEAN PRIMARY KEY DEFAULT TRUE CHECK (singleton),"
            "  version INTEGER NOT NULL,"
            "  applied_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP)"))) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

// Devuelve la versión instalada, o -1 si la base es virgen (la tabla no
// existe o está vacía). queryOk distingue ese caso normal de un fallo real
// de la consulta: si se colapsaran ambos en -1, un problema de permisos o de
// red se convertiría en un reset destructivo.
//
// Se usa to_regclass() y no un SELECT directo contra schema_version porque
// en PostgreSQL una sentencia que falla DENTRO de una transacción aborta la
// transacción completa ("current transaction is aborted"). to_regclass
// devuelve NULL cuando la relación no existe, sin fallar.
int readInstalledVersion(QSqlDatabase &db, bool &queryOk, QString &errorMessage)
{
    queryOk = true;

    QSqlQuery existsQuery(db);
    if (!existsQuery.exec(QStringLiteral("SELECT to_regclass('public.schema_version')"))
        || !existsQuery.next()) {
        errorMessage = existsQuery.lastError().text();
        queryOk = false;
        return -1;
    }
    if (existsQuery.value(0).isNull())
        return -1;

    QSqlQuery versionQuery(db);
    if (!versionQuery.exec(QStringLiteral("SELECT version FROM schema_version WHERE singleton"))) {
        errorMessage = versionQuery.lastError().text();
        queryOk = false;
        return -1;
    }
    return versionQuery.next() ? versionQuery.value(0).toInt() : -1;
}

// ¿Ya hay un esquema puesto? Se pregunta por vehicles, que es la tabla
// central del negocio y existe en todas las versiones del esquema. Sirve
// para saber si el reset va a destruir datos incluso cuando schema_version
// no existe todavía, que es justo el caso de las bases creadas antes de que
// hubiera versionado.
bool hasExistingSchema(QSqlDatabase &db)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT to_regclass('public.vehicles')")) || !query.next())
        return false;
    return !query.value(0).isNull();
}

bool writeVersion(QSqlDatabase &db, int version, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO schema_version (singleton, version) VALUES (TRUE, :version) "
        "ON CONFLICT (singleton) DO UPDATE "
        "SET version = EXCLUDED.version, applied_at = CURRENT_TIMESTAMP"));
    query.bindValue(QStringLiteral(":version"), version);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

// UMA vigente usada por la regla de validación de pagos en efectivo (Paso 1
// del wizard de vehículos: Contado bloqueado si el precio >= 3210 * UMA). Se
// siembra aquí (no en DevSeeder) porque es dato de negocio, no de prueba, y
// debe existir también en producción.
//
// Solo corre cuando hubo reset, y en ese momento global_configurations acaba
// de recrearse vacía, así que en la práctica el INSERT siempre inserta. El
// ON CONFLICT DO NOTHING se mantiene para que re-ejecutar run() dentro del
// mismo proceso no falle por llave duplicada.
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

SchemaInitializer::Result SchemaInitializer::run(QSqlDatabase &db, bool allowDestructiveReset)
{
    Result result;

    // La lectura va FUERA de la transacción: en autocommit, una consulta que
    // falla no deja nada envenenado detrás.
    bool versionQueryOk = false;
    QString versionError;
    const int installedVersion = readInstalledVersion(db, versionQueryOk, versionError);
    if (!versionQueryOk) {
        result.errorMessage = QStringLiteral("No se pudo leer la versión del esquema: %1")
                                   .arg(versionError);
        return result;
    }

    result.previousVersion = installedVersion;

    if (installedVersion == kSchemaVersion) {
        result.ok = true;
        result.applied = false;
        return result;
    }

    // Se consulta antes de tocar nada: después del script la respuesta
    // siempre sería "sí" y no diría nada útil.
    result.hadExistingSchema = hasExistingSchema(db);

    if (!allowDestructiveReset) {
        result.errorMessage =
            QStringLiteral("El esquema de la base está en la versión %1 y esta build espera la "
                           "%2. Aplicarlo implica BORRAR todos los datos, y el reset automático "
                           "está desactivado (SCHEMA_AUTO_RESET).\n\n"
                           "Si de verdad quieres regenerar la base, pon SCHEMA_AUTO_RESET=true "
                           "en el archivo .env y vuelve a abrir la aplicación.")
                .arg(installedVersion)
                .arg(kSchemaVersion);
        return result;
    }

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

    // PostgreSQL tiene DDL transaccional: los DROP y CREATE de abajo
    // participan en esta transacción y se deshacen con el rollback. Por eso
    // la escritura de la versión puede ir dentro -- si algo falla a mitad,
    // se revierten esquema y versión a la vez, y nunca queda una base con el
    // esquema a medias marcada como versión buena. (Este diseño no
    // sobreviviría a un port a MySQL, que hace commit implícito en cada DDL.)
    QString stepError;
    if (!ensureVersionTable(db, stepError)) {
        result.errorMessage = QStringLiteral("Error creando la tabla schema_version: %1").arg(stepError);
        db.rollback();
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

    if (!seedUmaConfig(db, stepError)) {
        result.errorMessage = QStringLiteral("Error sembrando configuración UMA_DIARIA: %1").arg(stepError);
        db.rollback();
        return result;
    }

    if (!writeVersion(db, kSchemaVersion, stepError)) {
        result.errorMessage = QStringLiteral("Error registrando la versión del esquema: %1").arg(stepError);
        db.rollback();
        return result;
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1").arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    result.applied = true;
    return result;
}
