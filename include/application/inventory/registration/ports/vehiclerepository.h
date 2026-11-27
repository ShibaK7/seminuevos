#ifndef APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLEREPOSITORY_H
#define APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLEREPOSITORY_H

#include "domain/finance/value_objects/cashaccount.h"

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
        // Una compra de contado y su cuenta (Caja Chica o Cantidad Banco) no
        // tiene con qué pagarla: no se guardó nada. Lo que tiene, en
        // centésimos.
        bool insufficientFunds = false;
        domain::CashAccount fundsAccount = domain::CashAccount::PettyCash;
        qint64 availableHundredths = 0;
        QString errorMessage;
    };

    // Lo que se esperaba encontrar al corregir una unidad.
    struct Expected
    {
        // La versión de la unidad que se leyó.
        QString vehicleVersion;
        // Si el usuario cambió los datos de la contraparte. Si no, la unidad
        // se queda ligada a la que tiene y esa no se escribe: otro usuario pudo
        // corregirla mientras tanto desde otra operación de la misma persona.
        bool counterpartyEdited = false;
        // La versión de la contraparte que se leyó. Corregirla en su lugar
        // (misma identificación) solo procede si sigue siendo esa.
        QString counterpartyVersion;
    };

    struct UpdateOutcome
    {
        bool ok = false;
        // El folio ya no existe.
        bool notFound = false;
        // Su versión, o la de su contraparte, ya no es la que se leyó: otro
        // usuario la cambió.
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
        // Corregir la compra (precio, tipo o método) saca más de lo que hay
        // en su cuenta: no se guardó nada.
        bool insufficientFunds = false;
        domain::CashAccount fundsAccount = domain::CashAccount::PettyCash;
        qint64 availableHundredths = 0;
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

    // Guarda todo o nada y asigna el folio a la unidad. Una compra de contado
    // saca su precio de Caja Chica o Cantidad Banco (CashFlowRules) en la
    // misma transacción; sin saldo no se guarda.
    virtual SaveOutcome add(domain::Vehicle &vehicle) = 0;

    // Corrige la unidad ya guardada con el folio de `vehicle`, todo o nada, y
    // solo si sigue como se leyó (`expected`). Nunca cambia su rama, su estado
    // ni el costo de mantenimientos: ese lo acumula el historial. Si cambian
    // el precio, el tipo o el método de la compra, ajusta las cuentas de
    // dinero en la misma transacción; sin saldo no se guarda.
    virtual UpdateOutcome update(domain::Vehicle &vehicle, const Expected &expected) = 0;

protected:
    VehicleRepository() = default;
    VehicleRepository(const VehicleRepository &) = default;
    VehicleRepository &operator=(const VehicleRepository &) = default;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLEREPOSITORY_H
