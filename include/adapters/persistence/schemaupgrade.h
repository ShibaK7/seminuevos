#ifndef ADAPTERS_PERSISTENCE_SCHEMAUPGRADE_H
#define ADAPTERS_PERSISTENCE_SCHEMAUPGRADE_H

#include <QString>

class QSqlDatabase;

// Lleva una base creada con un init-db/ anterior hasta el esquema que espera
// esta versión de la app. Docker solo corre init-db/ con el volumen vacío, así
// que sin esto, quien jala el código sin recrear su base vería fallar el
// login (users.active o users.last_login no existirían) y no habría dónde
// llevar los saldos (cash_accounts y cash_movements).
//
// Cada paso revisa antes si hace falta: en una base al día no cambia nada ni
// pide candados sobre las tablas. Lo que agrega es lo mismo que ya trae
// 00_schema.sql para las bases nuevas.
namespace SchemaUpgrade
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result run(QSqlDatabase &db);
} // namespace SchemaUpgrade

#endif // ADAPTERS_PERSISTENCE_SCHEMAUPGRADE_H
