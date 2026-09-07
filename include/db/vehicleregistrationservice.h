#ifndef VEHICLEREGISTRATIONSERVICE_H
#define VEHICLEREGISTRATIONSERVICE_H

#include <QString>

class QSqlDatabase;
struct VehicleDraft;

// Inserta un vehículo completo (vehicles + vehicle_acquisitions +
// vehicle_conditions + vehicle_condition_items + vehicle_images +
// vehicle_documents) en una sola transacción atómica, siguiendo el mismo
// patrón que DevSeeder::run/SchemaInitializer::run (db.transaction() ->
// inserts -> rollback en el primer error -> commit).
//
// IMPORTANTE: los PendingImage/PendingDocument del VehicleDraft deben traer
// en sourcePath la ruta RELATIVA ya devuelta por LocalFileStorageManager
// (no la ruta original del archivo en disco del usuario) -- este servicio
// solo hace inserts, no toca el sistema de archivos.
namespace VehicleRegistrationService
{
struct Result
{
    bool ok = false;
    QString errorMessage;
    int folio = -1;
};

Result insertVehicle(QSqlDatabase &db, const VehicleDraft &draft);
} // namespace VehicleRegistrationService

#endif // VEHICLEREGISTRATIONSERVICE_H
