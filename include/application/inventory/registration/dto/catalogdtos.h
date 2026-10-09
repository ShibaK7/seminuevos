#ifndef APPLICATION_INVENTORY_REGISTRATION_DTO_CATALOGDTOS_H
#define APPLICATION_INVENTORY_REGISTRATION_DTO_CATALOGDTOS_H

#include "application/common/dto/catalogoptiondto.h"

#include <QList>
#include <QString>

#include <optional>

namespace application {

// Un renglón del checklist de condición del Paso 2.
struct ChecklistItemDto
{
    int id = -1;
    QString category;
    QString element;
};

// Todo lo que el asistente de registro necesita leer antes de capturar. Se
// pide de una vez, fuera del hilo de la interfaz.
struct RegistrationLookupsDto
{
    QList<CatalogOptionDto> vehicleTypes;   // categorías raíz
    QList<CatalogOptionDto> vehicleSubtypes; // todas, con su parentId
    QList<CatalogOptionDto> brands;
    QList<CatalogOptionDto> fuelTypes;
    QList<ChecklistItemDto> checklist;
    std::optional<double> umaDailyValue;
    QString errorMessage; // si algo no se pudo leer
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_DTO_CATALOGDTOS_H
