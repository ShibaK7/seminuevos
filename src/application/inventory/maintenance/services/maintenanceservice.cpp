#include "application/inventory/maintenance/services/maintenanceservice.h"

#include "domain/inventory/model/maintenancerecord.h"
#include "domain/inventory/rules/vehiclerecordaccess.h"

namespace application {

namespace {

bool canUseHistory(domain::UserRole role)
{
    return domain::VehicleRecordAccess(role).canSeeMaintenanceHistory();
}

} // namespace

MaintenanceService::MaintenanceService(MaintenanceRepository &maintenances,
                                       ReferenceDataReader &referenceData,
                                       MaintenanceReceiptGenerator &receipts)
    : m_maintenances(maintenances)
    , m_referenceData(referenceData)
    , m_receipts(receipts)
{
}

MaintenanceFormLookupsResult MaintenanceService::formLookups(domain::UserRole role) const
{
    using Status = MaintenanceFormLookupsResult::Status;
    MaintenanceFormLookupsResult result;
    if (!canUseHistory(role)) {
        result.status = Status::Denied;
        return result;
    }

    QString error;
    result.maintenanceTypes = m_referenceData.maintenanceTypes(&error);
    if (!error.isEmpty()) {
        result.status = Status::Failed;
        result.maintenanceTypes.clear();
        result.errorMessage = error;
        return result;
    }
    result.status = Status::Loaded;
    return result;
}

MaintenanceHistoryResult MaintenanceService::history(int folio, domain::UserRole role) const
{
    using Status = MaintenanceHistoryResult::Status;
    MaintenanceHistoryResult result;
    if (!canUseHistory(role)) {
        result.status = Status::Denied;
        return result;
    }

    QString error;
    result.items = m_maintenances.listForVehicle(folio, &error);
    if (!error.isEmpty()) {
        result.status = Status::Failed;
        result.items.clear();
        result.errorMessage = error;
        return result;
    }
    result.status = Status::Loaded;
    return result;
}

MaintenanceAddResult MaintenanceService::add(const NewMaintenanceDto &maintenance,
                                             domain::UserRole role) const
{
    using Status = MaintenanceAddResult::Status;
    MaintenanceAddResult result;
    if (!canUseHistory(role)) {
        result.status = Status::Denied;
        return result;
    }

    const domain::MaintenanceRecord record(maintenance.vehicleFolio, maintenance.date,
                                           maintenance.maintenanceTypeId.value_or(-1),
                                           maintenance.cost, maintenance.description);
    const domain::ValidationResult validation = record.validate();
    if (!validation.isValid()) {
        result.status = Status::Rejected;
        result.validation = validation;
        return result;
    }

    const MaintenanceRepository::AddOutcome outcome = m_maintenances.add(record);
    if (!outcome.ok) {
        result.status = Status::Failed;
        result.errorMessage = outcome.errorMessage;
        return result;
    }
    result.status = Status::Saved;
    result.id = outcome.id;
    result.maintenanceCostTotal = outcome.maintenanceCostTotal;
    return result;
}

MaintenanceDescriptionUpdateResult MaintenanceService::updateDescription(
    int id, const QString &description, domain::UserRole role) const
{
    using Status = MaintenanceDescriptionUpdateResult::Status;
    MaintenanceDescriptionUpdateResult result;
    if (!canUseHistory(role)) {
        result.status = Status::Denied;
        return result;
    }

    const domain::ValidationResult validation =
        domain::MaintenanceRecord::validateDescription(description);
    if (!validation.isValid()) {
        result.status = Status::Rejected;
        result.validation = validation;
        return result;
    }

    QString error;
    if (!m_maintenances.updateDescription(id, description.trimmed(), &error)) {
        result.status = Status::Failed;
        result.errorMessage = error;
        return result;
    }
    result.status = Status::Updated;
    return result;
}

MaintenanceReceiptGenerator::Outcome MaintenanceService::generateReceipt(
    const MaintenanceItemDto &maintenance, const QString &outputPath, domain::UserRole role) const
{
    if (!canUseHistory(role)) {
        MaintenanceReceiptGenerator::Outcome denied;
        denied.errorMessage = QStringLiteral("Tu rol no puede imprimir recibos de mantenimiento.");
        return denied;
    }
    return m_receipts.generate(maintenance, outputPath);
}

MaintenanceReceiptMarkResult MaintenanceService::markReceiptPrinted(int id,
                                                                   domain::UserRole role) const
{
    using Status = MaintenanceReceiptMarkResult::Status;
    MaintenanceReceiptMarkResult result;
    if (!canUseHistory(role)) {
        result.status = Status::Denied;
        return result;
    }
    QString error;
    if (!m_maintenances.markReceiptPrinted(id, &error)) {
        result.status = Status::Failed;
        result.errorMessage = error;
        return result;
    }
    result.status = Status::Marked;
    return result;
}

} // namespace application
