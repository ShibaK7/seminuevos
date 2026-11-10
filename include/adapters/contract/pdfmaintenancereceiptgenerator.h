#ifndef ADAPTERS_CONTRACT_PDFMAINTENANCERECEIPTGENERATOR_H
#define ADAPTERS_CONTRACT_PDFMAINTENANCERECEIPTGENERATOR_H

#include "application/inventory/maintenance/ports/maintenancereceiptgenerator.h"

// Adaptador del puerto MaintenanceReceiptGenerator: genera el PDF del recibo
// a partir de la plantilla HTML empaquetada como recurso
// (:/templates/receipt_maintenance.html), sustituyendo sus marcas [Fecha],
// [Costo], [Tipo_Mantenimiento] y [Descripción] con los datos del renglón.
class PdfMaintenanceReceiptGenerator final : public application::MaintenanceReceiptGenerator
{
public:
    Outcome generate(const application::MaintenanceItemDto &maintenance,
                     const QString &outputPath) override;

    // La plantilla con los datos del mantenimiento, lista para dibujarse. Vacía
    // si no se pudo llenar completa, con la causa en `error`.
    static QString fillTemplate(const application::MaintenanceItemDto &maintenance,
                                QString *error);
};

#endif // ADAPTERS_CONTRACT_PDFMAINTENANCERECEIPTGENERATOR_H
