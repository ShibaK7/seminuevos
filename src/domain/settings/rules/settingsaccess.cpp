#include "domain/settings/rules/settingsaccess.h"

namespace domain {

SettingsAccess::SettingsAccess(UserRole role)
    : m_role(role)
{
}

bool SettingsAccess::canManageSystemParameters() const
{
    return m_role == UserRole::Admin;
}

bool SettingsAccess::canManageUsers() const
{
    return m_role == UserRole::Admin;
}

bool SettingsAccess::canOpenSettings() const
{
    return canManageSystemParameters() || canManageUsers();
}

} // namespace domain
