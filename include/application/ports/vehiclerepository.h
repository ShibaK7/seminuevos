#ifndef APPLICATION_PORTS_VEHICLEREPOSITORY_H
#define APPLICATION_PORTS_VEHICLEREPOSITORY_H

#include <QString>

namespace domain {
class Vehicle;
}

namespace application {

// Puerto: dónde se guarda una unidad. Un repositorio por agregado (la unidad
// con su contraparte, condiciones, inspección, fotos y documentos), no uno por
// tabla: guardar la unidad es una sola operación atómica, y la transacción es
// detalle del adaptador (SqlVehicleRepository), no del caso de uso.
class VehicleRepository
{
public:
    struct SaveOutcome
    {
        bool ok = false;
        int folio = -1;
        // El VIN ya existía (lo rechazó la restricción UNIQUE de la base).
        bool duplicateSerialNumber = false;
        QString errorMessage;
    };

    virtual ~VehicleRepository() = default;

    // Consulta previa para no copiar archivos de una unidad que la base va a
    // rechazar. Si la consulta falla devuelve false y deja el error en `error`.
    virtual bool serialNumberExists(const QString &serialNumber, QString *error) = 0;

    // Guarda todo o nada y asigna el folio a la unidad.
    virtual SaveOutcome add(domain::Vehicle &vehicle) = 0;

protected:
    VehicleRepository() = default;
    VehicleRepository(const VehicleRepository &) = default;
    VehicleRepository &operator=(const VehicleRepository &) = default;
};

} // namespace application

#endif // APPLICATION_PORTS_VEHICLEREPOSITORY_H
