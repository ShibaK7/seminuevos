#ifndef APPLICATION_INVENTORY_REGISTRATION_DTO_CATALOGDTOS_H
#define APPLICATION_INVENTORY_REGISTRATION_DTO_CATALOGDTOS_H

#include "application/common/dto/catalogoptiondto.h"
#include "domain/common/value_objects/cashlimitterms.h"
#include "domain/finance/value_objects/cashaccount.h"

#include <QList>
#include <QMap>
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
    // La UMA y el tope de efectivo. Sin UMA, umaDailyValue queda en 0.
    domain::CashLimitTerms cashLimit;
    // Lo que hay en Caja Chica y Cantidad Banco, en centésimos: una compra de
    // contado no puede pasar de lo que hay en la cuenta de su método. Vacío si
    // no se pudo leer (entonces lo decide el guardado).
    QMap<domain::CashAccount, qint64> cashAvailable;
    QString errorMessage; // si algo no se pudo leer
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_DTO_CATALOGDTOS_H
