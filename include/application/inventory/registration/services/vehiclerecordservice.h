#ifndef APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H
#define APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H

#include "application/common/ports/filestorage.h"
#include "application/inventory/registration/dto/recorddtos.h"
#include "application/inventory/registration/ports/referencedatareader.h"
#include "application/inventory/registration/ports/vehiclerecordreader.h"
#include "application/inventory/registration/ports/vehiclerepository.h"
#include "domain/common/value_objects/enums.h"

namespace application {

// Caso de uso "consultar y corregir el expediente de un vehículo" (US-03.3 y
// US-03.4). Vive junto al alta, en registration/, porque el expediente es el
// mismo formulario: comparte sus DTO por paso, su mapeo al dominio y sus
// reglas de validación.
//
// Solo guarda referencias a sus puertos, así que se puede llamar desde el hilo
// de trabajo.
class VehicleRecordService final
{
public:
    VehicleRecordService(VehicleRecordReader &records, VehicleRepository &vehicles,
                         FileStorage &files, ReferenceDataReader &referenceData);

    // Qué ve y qué puede hacer ese rol en el expediente. Pura (sin E/S): se
    // puede llamar en el hilo de la interfaz.
    RecordAccessDto accessFor(domain::UserRole role) const;

    // El expediente de un folio, listo para pintarse: rutas absolutas, las
    // miniaturas de las fotos y exactamente una portada. A quien su rol no le
    // deja ver la operación, le llega sin ella. Hace E/S (base y disco):
    // correrlo fuera del hilo de la interfaz.
    RecordLoadResult load(int folio, domain::UserRole role) const;

    // Guarda lo corregido (#34), en este orden: el rol, las reglas del
    // dominio (sin tocar disco ni base), que la unidad siga como se leyó (si
    // otro la cambió, Conflict antes de copiar nada) y que el VIN no sea de
    // otra. Después conserva los archivos que ya estaban, copia los nuevos y
    // guarda todo o nada; si eso falla, borra solo lo que copió, y si
    // confirma, borra del almacén lo que dejó de usarse. Hace E/S.
    RecordUpdateResult update(const VehicleUpdateDto &update, domain::UserRole role) const;

private:
    VehicleRecordReader &m_records;
    VehicleRepository &m_vehicles;
    FileStorage &m_files;
    ReferenceDataReader &m_referenceData;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H
