#ifndef APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLERECORDREADER_H
#define APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLERECORDREADER_H

#include "application/inventory/registration/dto/recorddtos.h"

#include <QString>

#include <optional>

namespace application {

// Puerto: el expediente de una unidad por su folio, tal como se guardó. Es
// una proyección de lectura, como InventoryReader: no arma un Vehicle ni pasa
// por sus reglas, porque solo se va a pintar. Corregir el expediente sí pasa
// por el dominio, a través de VehicleRepository.
class VehicleRecordReader
{
public:
    virtual ~VehicleRecordReader() = default;

    // Todo lo del expediente leído de una sola vez, con su versión. nullopt si
    // el folio no existe (error vacío) o si la consulta falla (con la causa en
    // `error`). Las rutas de archivos son las de la base, relativas al
    // almacén; las miniaturas van vacías.
    virtual std::optional<VehicleRecordDto> findByFolio(int folio, QString *error) = 0;

protected:
    VehicleRecordReader() = default;
    VehicleRecordReader(const VehicleRecordReader &) = default;
    VehicleRecordReader &operator=(const VehicleRecordReader &) = default;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_PORTS_VEHICLERECORDREADER_H
