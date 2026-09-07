#ifndef VEHICLEREPOSITORY_H
#define VEHICLEREPOSITORY_H

#include <QString>

class QSqlDatabase;

namespace domain {
class Vehicle;
}

// Persistencia del vehículo y de todo lo que cuelga de él.
//
// Un registro toca seis tablas (vehicles, la subtabla de su rama,
// vehicle_conditions, vehicle_inspection, vehicle_images y
// vehicle_documents) y debe ser atómico: o queda todo o no queda nada. Eso ya
// es trabajo de mapeador, no de "un objeto, una fila", que es la otra razón
// por la que esto no vive dentro de Vehicle.
//
// El reparto entre la tabla base y la subtabla lo resuelve un visitante, no
// un dynamic_cast: así, si mañana aparece una tercera forma de adquirir una
// unidad, el compilador exige atenderla en vez de dejar que se olvide en
// silencio -- que es justo lo que había pasado con la consignación.
class VehicleRepository
{
public:
    struct Result
    {
        bool ok = false;
        int folio = -1;
        QString errorMessage;
    };

    explicit VehicleRepository(QSqlDatabase &db);

    // Guarda la unidad completa en una sola transacción. No es const porque
    // asigna a la unidad el folio y el id de contraparte que devuelve la base.
    Result save(domain::Vehicle &vehicle);

private:
    QSqlDatabase &m_db;
};

#endif // VEHICLEREPOSITORY_H
