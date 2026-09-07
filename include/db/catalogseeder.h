#ifndef CATALOGSEEDER_H
#define CATALOGSEEDER_H

#include <QString>

class QSqlDatabase;

// Siembra catálogos DUMMY (tipos/subtipos de vehículo, marcas, tipos de
// combustible) para que los combos del wizard de registro de vehículo
// (US-03.2, Step1DetailsView/Step2ConditionView) no queden vacíos mientras
// el negocio entrega la lista real -- sin esto vehicle_categories_cat,
// brands_cat y fuel_type_cat quedan sin filas y esos combos no tienen nada
// que seleccionar. Mismo criterio que DevSeeder: solo corre si
// SEED_TEST_USERS=true en .env (datos de prueba, no de producción).
// Reemplazar/eliminar este seeder en cuanto exista el catálogo oficial.
namespace CatalogSeeder
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result run(QSqlDatabase &db);
} // namespace CatalogSeeder

#endif // CATALOGSEEDER_H
