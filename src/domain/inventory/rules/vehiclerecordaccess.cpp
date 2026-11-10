#include "domain/inventory/rules/vehiclerecordaccess.h"

namespace domain {

VehicleRecordAccess::VehicleRecordAccess(UserRole role)
    : m_role(role)
{
}

bool VehicleRecordAccess::canEdit() const
{
    return m_role == UserRole::Admin;
}

bool VehicleRecordAccess::canSeeDealTerms() const
{
    return m_role == UserRole::Admin;
}

bool VehicleRecordAccess::canSeeMaintenanceHistory() const
{
    return m_role == UserRole::Admin;
}

} // namespace domain
