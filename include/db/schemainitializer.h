#ifndef SCHEMAINITIALIZER_H
#define SCHEMAINITIALIZER_H

#include <QString>

class QSqlDatabase;

// Aplica db/001_init_schema.sql (empaquetado como recurso Qt en
// :/db/001_init_schema.sql) contra la base de datos. El script es
// idempotente (CREATE TABLE IF NOT EXISTS, INSERT ... ON CONFLICT DO
// NOTHING), así que correrlo en cada arranque de la app es seguro: no
// depende de que Docker aplique nada una sola vez, cualquier compañero que
// jale código nuevo lo recibe automáticamente al abrir la app.
namespace SchemaInitializer
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result run(QSqlDatabase &db);
} // namespace SchemaInitializer

#endif // SCHEMAINITIALIZER_H
