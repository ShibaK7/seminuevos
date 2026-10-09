#ifndef APPLICATION_INVENTORY_REGISTRATION_SERVICES_REGISTRATIONMAPPING_H
#define APPLICATION_INVENTORY_REGISTRATION_SERVICES_REGISTRATIONMAPPING_H

// Privado de la capa de aplicación (vive en src/, no en include/): vuelca los
// DTOs del asistente sobre un VehicleBuilder. Es lo que antes hacían los
// applyTo() de cada vista, que obligaban a los widgets a conocer el dominio.
//
// Funciones libres y no una clase "Mapper": no tienen estado ni invariantes.

#include "application/inventory/registration/dto/registrationdtos.h"

namespace domain {
class VehicleBuilder;
}

namespace application::mapping {

// Devuelve los rechazos de los setters que el builder no registra por su
// cuenta (los de la contraparte), para que no se pierdan en silencio.
domain::ValidationResult applyDetails(domain::VehicleBuilder &builder,
                                      const VehicleDetailsDto &details,
                                      std::optional<double> umaDailyValue);

domain::ValidationResult applyConditions(domain::VehicleBuilder &builder,
                                         const VehicleConditionsDto &conditions);

void applyFiles(domain::VehicleBuilder &builder, const VehicleFilesDto &files);

} // namespace application::mapping

#endif // APPLICATION_INVENTORY_REGISTRATION_SERVICES_REGISTRATIONMAPPING_H
