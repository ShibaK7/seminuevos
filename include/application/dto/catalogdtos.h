#ifndef APPLICATION_DTO_CATALOGDTOS_H
#define APPLICATION_DTO_CATALOGDTOS_H

#include <QList>
#include <QString>

#include <optional>

namespace application {

// Una opción de un catálogo (marca, tipo, combustible...). parentId sirve para
// los subtipos de vehículo: -1 si la opción no cuelga de otra.
struct CatalogOptionDto
{
    int id = -1;
    QString name;
    int parentId = -1;
};

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

#endif // APPLICATION_DTO_CATALOGDTOS_H
