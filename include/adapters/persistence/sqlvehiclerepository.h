#ifndef ADAPTERS_PERSISTENCE_SQLVEHICLEREPOSITORY_H
#define ADAPTERS_PERSISTENCE_SQLVEHICLEREPOSITORY_H

#include "application/ports/vehiclerepository.h"

#include <QString>

class ConnectionPool;
class QSqlDatabase;

namespace domain {
class Vehicle;
}

// Adaptador del puerto VehicleRepository sobre PostgreSQL: persistencia del
// vehículo y de todo lo que cuelga de él.
//
// Un registro toca seis tablas (vehicles, la subtabla de su rama,
// vehicle_conditions, vehicle_inspection, vehicle_images y vehicle_documents)
// y debe ser atómico: o queda todo o no queda nada. La transacción vive aquí,
// en el adaptador del agregado, y no en el caso de uso: el servicio no sabe
// que existe QSqlDatabase.
//
// El reparto entre la tabla base y la subtabla lo resuelve un visitante, no
// un dynamic_cast: si mañana aparece una tercera forma de adquirir una
// unidad, el compilador exige atenderla en vez de dejar que se olvide en
// silencio -- que es justo lo que había pasado con la consignación.
//
// Pide la conexión en cada llamada (no la guarda), así que sirve desde el hilo
// de trabajo.
class SqlVehicleRepository final : public application::VehicleRepository
{
public:
    explicit SqlVehicleRepository(ConnectionPool &pool);

    bool serialNumberExists(const QString &serialNumber, QString *error) override;

    // No es const porque asigna a la unidad el folio y el id de contraparte
    // que devuelve la base.
    SaveOutcome add(domain::Vehicle &vehicle) override;

private:
    static SaveOutcome save(QSqlDatabase &db, domain::Vehicle &vehicle);

    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_SQLVEHICLEREPOSITORY_H
