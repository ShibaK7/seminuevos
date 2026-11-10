#ifndef APPLICATION_INVENTORY_MAINTENANCE_DTO_MAINTENANCEDTOS_H
#define APPLICATION_INVENTORY_MAINTENANCE_DTO_MAINTENANCEDTOS_H

#include "application/common/dto/catalogoptiondto.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace application {

// DTOs del historial de mantenimientos de una unidad (US-03.3). Igual que los
// del alta, son structs planos.

// Lo que el formulario de "Agregar Mantenimiento" necesita leer antes de
// abrirse.
struct MaintenanceFormLookupsResult
{
    enum class Status {
        Loaded, // maintenanceTypes trae el catálogo
        Denied, // el rol no puede agregar mantenimientos
        Failed, // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    QList<CatalogOptionDto> maintenanceTypes;
    QString errorMessage; // en Failed
};

// Un renglón del historial, listo para pintarse.
struct MaintenanceItemDto
{
    int id = -1;
    QDate date;
    QString typeName;
    double cost = 0.0;
    QString description;
    // Si ya se imprimió su recibo alguna vez. Solo informa: se puede volver a
    // imprimir.
    bool receiptPrinted = false;
};

struct MaintenanceHistoryResult
{
    enum class Status {
        Loaded, // items trae el historial, del más reciente al más viejo
        Denied, // el rol no ve el historial
        Failed, // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    QList<MaintenanceItemDto> items;
    QString errorMessage; // en Failed
};

// Lo capturado en "Agregar Mantenimiento". El costo va como el texto que se
// escribió: el dominio decide si es un número que cabe en la columna.
struct NewMaintenanceDto
{
    int vehicleFolio = -1;
    QDate date;
    std::optional<int> maintenanceTypeId; // sin elegir
    QString cost;
    QString description;
};

struct MaintenanceAddResult
{
    enum class Status {
        Saved,    // id es el renglón nuevo
        Rejected, // validation dice qué campos corregir; no se tocó la base
        Denied,   // el rol no puede agregar mantenimientos
        Failed,   // falló la base y no se guardó nada; se puede reintentar
    };
    Status status = Status::Failed;
    int id = -1;
    // El total de mantenimientos de la unidad después de sumar este, o
    // nullopt si la unidad no tiene el renglón de su operación (compra o
    // consignación) donde acumularlo.
    std::optional<double> maintenanceCostTotal;
    domain::ValidationResult validation; // en Rejected
    QString errorMessage;                 // en Failed
};

// Corregir la descripción de un mantenimiento ya guardado.
struct MaintenanceDescriptionUpdateResult
{
    enum class Status {
        Updated,  // quedó guardada
        Rejected, // validation dice por qué; no se tocó la base
        Denied,   // el rol no puede corregir el historial
        Failed,   // falló la base o el renglón ya no existe; no se guardó
    };
    Status status = Status::Failed;
    domain::ValidationResult validation; // en Rejected
    QString errorMessage;                 // en Failed
};

// Anotar que el recibo de un mantenimiento ya se imprimió (#47).
struct MaintenanceReceiptMarkResult
{
    enum class Status {
        Marked, // quedó como impreso
        Denied, // el rol no puede imprimir recibos
        Failed, // falló la base; el recibo sí se generó
    };
    Status status = Status::Failed;
    QString errorMessage; // en Failed
};

} // namespace application

#endif // APPLICATION_INVENTORY_MAINTENANCE_DTO_MAINTENANCEDTOS_H
