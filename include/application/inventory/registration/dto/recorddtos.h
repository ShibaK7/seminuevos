#ifndef APPLICATION_INVENTORY_REGISTRATION_DTO_RECORDDTOS_H
#define APPLICATION_INVENTORY_REGISTRATION_DTO_RECORDDTOS_H

namespace application {

// DTOs del expediente de un vehículo: consultarlo y corregirlo (US-03.3 y
// US-03.4). Igual que los del alta, son structs planos.

// Qué deja ver y hacer el rol de quien entró. Lo decide VehicleRecordAccess
// en el dominio; la pantalla solo recibe los booleanos, porque la lógica del
// front no puede incluir las reglas del dominio.
struct RecordAccessDto
{
    bool canEdit = false;
    bool showsDealTerms = false;
    bool showsMaintenanceHistory = false;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_DTO_RECORDDTOS_H
