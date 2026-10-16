#ifndef APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H
#define APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H

#include "application/common/ports/filestorage.h"
#include "application/inventory/registration/dto/recorddtos.h"
#include "application/inventory/registration/ports/vehiclerecordreader.h"
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
    VehicleRecordService(VehicleRecordReader &records, FileStorage &files);

    // Qué ve y qué puede hacer ese rol en el expediente. Pura (sin E/S): se
    // puede llamar en el hilo de la interfaz.
    RecordAccessDto accessFor(domain::UserRole role) const;

    // El expediente de un folio, listo para pintarse: rutas absolutas, las
    // miniaturas de las fotos y exactamente una portada. A quien su rol no le
    // deja ver la operación, le llega sin ella. Hace E/S (base y disco):
    // correrlo fuera del hilo de la interfaz.
    RecordLoadResult load(int folio, domain::UserRole role) const;

private:
    VehicleRecordReader &m_records;
    FileStorage &m_files;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H
