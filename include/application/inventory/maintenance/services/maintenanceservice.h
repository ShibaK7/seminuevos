#ifndef APPLICATION_INVENTORY_MAINTENANCE_SERVICES_MAINTENANCESERVICE_H
#define APPLICATION_INVENTORY_MAINTENANCE_SERVICES_MAINTENANCESERVICE_H

#include "application/inventory/maintenance/dto/maintenancedtos.h"
#include "application/inventory/maintenance/ports/maintenancereceiptgenerator.h"
#include "application/inventory/maintenance/ports/maintenancerepository.h"
#include "application/inventory/registration/ports/referencedatareader.h"
#include "domain/common/value_objects/enums.h"

namespace application {

// Caso de uso "llevar el historial de mantenimientos de una unidad" (US-03.3,
// tareas #45 a #48). Cada operación exige primero que el rol pueda ver el
// historial (VehicleRecordAccess): ocultar la sección en el expediente no
// basta, y el presenter no puede preguntarle al dominio.
//
// Solo guarda referencias a sus puertos, así que se puede llamar desde el hilo
// de trabajo. Todas sus operaciones hacen E/S; salvo el recibo, correrlas
// fuera del hilo de la interfaz.
class MaintenanceService final
{
public:
    MaintenanceService(MaintenanceRepository &maintenances, ReferenceDataReader &referenceData,
                       MaintenanceReceiptGenerator &receipts);

    // Los catálogos del formulario de alta.
    MaintenanceFormLookupsResult formLookups(domain::UserRole role) const;

    // El historial de un folio.
    MaintenanceHistoryResult history(int folio, domain::UserRole role) const;

    // Agrega un mantenimiento (#46): el rol, las reglas del dominio (sin tocar
    // la base) y después el renglón nuevo junto con la suma al total de la
    // unidad, todo o nada.
    MaintenanceAddResult add(const NewMaintenanceDto &maintenance, domain::UserRole role) const;

    // Corrige la descripción de un mantenimiento ya guardado: el rol, la
    // regla del dominio y después la base. Hace E/S.
    MaintenanceDescriptionUpdateResult updateDescription(int id, const QString &description,
                                                         domain::UserRole role) const;

    // El recibo de un mantenimiento del historial (#47), en outputPath. Como
    // el PDF del contrato, corre en el hilo de la interfaz: QTextDocument y
    // QPrinter dibujan ahí, y no toca la base.
    MaintenanceReceiptGenerator::Outcome generateReceipt(const MaintenanceItemDto &maintenance,
                                                         const QString &outputPath,
                                                         domain::UserRole role) const;
    // Anota que ya se imprimió el recibo del mantenimiento `id`. Hace E/S:
    // correrlo fuera del hilo de la interfaz.
    MaintenanceReceiptMarkResult markReceiptPrinted(int id, domain::UserRole role) const;

private:
    MaintenanceRepository &m_maintenances;
    ReferenceDataReader &m_referenceData;
    MaintenanceReceiptGenerator &m_receipts;
};

} // namespace application

#endif // APPLICATION_INVENTORY_MAINTENANCE_SERVICES_MAINTENANCESERVICE_H
