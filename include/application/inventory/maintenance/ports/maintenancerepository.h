#ifndef APPLICATION_INVENTORY_MAINTENANCE_PORTS_MAINTENANCEREPOSITORY_H
#define APPLICATION_INVENTORY_MAINTENANCE_PORTS_MAINTENANCEREPOSITORY_H

#include "application/inventory/maintenance/dto/maintenancedtos.h"
#include "domain/inventory/model/maintenancerecord.h"

#include <QList>
#include <QString>

#include <optional>

namespace application {

// Puerto: el historial de mantenimientos de las unidades (vehicle_maintenance).
class MaintenanceRepository
{
public:
    virtual ~MaintenanceRepository() = default;

    // Los mantenimientos de un folio, del más reciente al más viejo. Si la
    // consulta falla devuelve vacío y deja la causa en `error`.
    virtual QList<MaintenanceItemDto> listForVehicle(int folio, QString *error) = 0;

    struct AddOutcome
    {
        bool ok = false;
        int id = -1;
        std::optional<double> maintenanceCostTotal; // ver MaintenanceAddResult
        QString errorMessage;
    };
    // Guarda un mantenimiento ya validado y, en la misma transacción, suma su
    // costo al total de mantenimientos de la operación de la unidad. Todo o
    // nada.
    virtual AddOutcome add(const domain::MaintenanceRecord &record) = 0;

    // Reemplaza la descripción del mantenimiento `id`, ya validada. Devuelve
    // false, con la causa en `error`, si falló la base o el renglón ya no
    // existe.
    virtual bool updateDescription(int id, const QString &description, QString *error) = 0;

    // Anota que el recibo del mantenimiento `id` ya se imprimió. Devuelve
    // false, con la causa en `error`, si falló la base o el renglón ya no
    // existe.
    virtual bool markReceiptPrinted(int id, QString *error) = 0;

protected:
    MaintenanceRepository() = default;
    MaintenanceRepository(const MaintenanceRepository &) = default;
    MaintenanceRepository &operator=(const MaintenanceRepository &) = default;
};

} // namespace application

#endif // APPLICATION_INVENTORY_MAINTENANCE_PORTS_MAINTENANCEREPOSITORY_H
