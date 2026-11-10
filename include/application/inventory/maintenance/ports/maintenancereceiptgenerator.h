#ifndef APPLICATION_INVENTORY_MAINTENANCE_PORTS_MAINTENANCERECEIPTGENERATOR_H
#define APPLICATION_INVENTORY_MAINTENANCE_PORTS_MAINTENANCERECEIPTGENERATOR_H

#include "application/inventory/maintenance/dto/maintenancedtos.h"

#include <QString>

namespace application {

// Puerto: cómo se produce el recibo de un mantenimiento (#47). Hoy, un PDF
// con QPrinter sobre la plantilla receipt_maintenance.html, igual que el
// contrato (ContractGenerator).
class MaintenanceReceiptGenerator
{
public:
    struct Outcome
    {
        bool ok = false;
        QString errorMessage;
    };

    virtual ~MaintenanceReceiptGenerator() = default;

    virtual Outcome generate(const MaintenanceItemDto &maintenance, const QString &outputPath) = 0;

protected:
    MaintenanceReceiptGenerator() = default;
    MaintenanceReceiptGenerator(const MaintenanceReceiptGenerator &) = default;
    MaintenanceReceiptGenerator &operator=(const MaintenanceReceiptGenerator &) = default;
};

} // namespace application

#endif // APPLICATION_INVENTORY_MAINTENANCE_PORTS_MAINTENANCERECEIPTGENERATOR_H
