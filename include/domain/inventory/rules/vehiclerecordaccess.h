#ifndef DOMAIN_INVENTORY_RULES_VEHICLERECORDACCESS_H
#define DOMAIN_INVENTORY_RULES_VEHICLERECORDACCESS_H

#include "domain/common/value_objects/enums.h"

namespace domain {

// Qué puede ver y hacer cada rol en el expediente de un vehículo (US-03.3 y
// US-03.4). Es el ÚNICO lugar que lo decide: la pantalla, el servicio y el
// historial de mantenimientos le preguntan a esta clase, y nadie más compara
// roles.
//
// Es una regla aparte, y no un método de Vehicle, porque no le pertenece a la
// unidad: depende de quién la consulta. Las tres preguntas hoy tienen la
// misma respuesta ("¿es Administrador?"), pero son decisiones distintas del
// negocio, y cada una puede cambiar por su lado (por ejemplo, que un vendedor
// vea el precio de venta pero no el de compra).
class VehicleRecordAccess
{
public:
    explicit VehicleRecordAccess(UserRole role);

    // "Editar" en el expediente (US-03.4).
    bool canEdit() const;
    // La tarjeta de la operación: propietario o vendedor, factura, precios,
    // mantenimientos y observaciones.
    bool canSeeDealTerms() const;
    // La sección del historial de mantenimientos.
    bool canSeeMaintenanceHistory() const;

private:
    UserRole m_role;
};

} // namespace domain

#endif // DOMAIN_INVENTORY_RULES_VEHICLERECORDACCESS_H
