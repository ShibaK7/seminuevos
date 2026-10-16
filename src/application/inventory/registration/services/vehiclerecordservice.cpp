#include "application/inventory/registration/services/vehiclerecordservice.h"

#include "domain/inventory/rules/vehiclerecordaccess.h"

namespace application {

RecordAccessDto VehicleRecordService::accessFor(domain::UserRole role) const
{
    const domain::VehicleRecordAccess access(role);
    RecordAccessDto dto;
    dto.canEdit = access.canEdit();
    dto.showsDealTerms = access.canSeeDealTerms();
    dto.showsMaintenanceHistory = access.canSeeMaintenanceHistory();
    return dto;
}

} // namespace application
