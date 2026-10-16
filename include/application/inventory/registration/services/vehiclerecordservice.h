#ifndef APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H
#define APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H

#include "application/inventory/registration/dto/recorddtos.h"
#include "domain/common/value_objects/enums.h"

namespace application {

// Caso de uso "consultar y corregir el expediente de un vehículo" (US-03.3 y
// US-03.4). Vive junto al alta, en registration/, porque el expediente es el
// mismo formulario: comparte sus DTO por paso, su mapeo al dominio y sus
// reglas de validación.
class VehicleRecordService final
{
public:
    // Qué ve y qué puede hacer ese rol en el expediente. Pura (sin E/S): se
    // puede llamar en el hilo de la interfaz.
    RecordAccessDto accessFor(domain::UserRole role) const;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLERECORDSERVICE_H
