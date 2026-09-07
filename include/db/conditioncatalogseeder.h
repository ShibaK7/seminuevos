#ifndef CONDITIONCATALOGSEEDER_H
#define CONDITIONCATALOGSEEDER_H

#include <QString>

class QSqlDatabase;

// Siembra vehicle_conditions_cat, el catálogo del checklist de condición del
// Paso 2 del wizard (derivado de las capturas de la app legacy, ver US-03.2).
//
// A diferencia de CatalogSeeder esto NO es dummy y NO va detrás de la
// bandera SEED_TEST_USERS: la UI construye sus renglones leyendo esta tabla,
// así que si queda vacía el Paso 2 sale en blanco en cualquier ambiente.
//
// Idempotente: hace upsert por item_key en cada arranque.
//   - ítem nuevo, o etiqueta/orden corregidos -> se propaga al siguiente
//     arranque, sin tocar el esquema ni perder datos.
//   - ítem ELIMINADO de la semilla -> NO se borra de la base. Para retirarlo
//     de verdad, quítalo de aquí y sube SchemaInitializer::kSchemaVersion;
//     el reset regenera el catálogo desde cero.
//
// Mientras exista este seeder la lista de C++ manda: no edites
// vehicle_conditions_cat a mano, se te va a pisar en el siguiente arranque.
namespace ConditionCatalogSeeder
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result run(QSqlDatabase &db);
} // namespace ConditionCatalogSeeder

#endif // CONDITIONCATALOGSEEDER_H
