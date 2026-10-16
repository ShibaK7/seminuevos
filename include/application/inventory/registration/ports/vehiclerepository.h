#ifndef APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLEREPOSITORY_H
#define APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLEREPOSITORY_H

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

    struct UpdateOutcome
    {
        bool ok = false;
        // El folio ya no existe.
        bool notFound = false;
        // Su versión ya no es la que se leyó: otro usuario la cambió.
        bool conflict = false;
        // El VIN ya es de otra unidad.
        bool duplicateSerialNumber = false;
        // Falló el COMMIT: no se sabe si quedó guardado, así que quien llama
        // no debe borrar ningún archivo (más vale uno huérfano que una
        // referencia rota).
        bool commitFailed = false;
        // La identificación capturada ya es de otra persona, con otro nombre:
        // el suyo. No se liga sin que el usuario lo revise.
        QString takenNationalIdOwner;
        QString errorMessage;
    };

    virtual ~VehicleRepository() = default;

    // Consulta previa para no copiar archivos de una unidad que la base va a
    // rechazar. Si la consulta falla devuelve false y deja el error en `error`.
    virtual bool serialNumberExists(const QString &serialNumber, QString *error) = 0;
    // Lo mismo sin contar a la unidad `folio`: al corregirla, su propio VIN no
    // es un duplicado.
    virtual bool serialNumberExistsElsewhere(const QString &serialNumber, int folio,
                                             QString *error) = 0;

    // Guarda todo o nada y asigna el folio a la unidad.
    virtual SaveOutcome add(domain::Vehicle &vehicle) = 0;

    // Corrige la unidad ya guardada con el folio de `vehicle`, todo o nada, y
    // solo si su versión sigue siendo `expectedVersion`. Nunca cambia su rama,
    // su estado ni el costo de mantenimientos: ese lo acumula el historial.
    virtual UpdateOutcome update(domain::Vehicle &vehicle, const QString &expectedVersion) = 0;

protected:
    VehicleRepository() = default;
    VehicleRepository(const VehicleRepository &) = default;
    VehicleRepository &operator=(const VehicleRepository &) = default;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLEREPOSITORY_H
