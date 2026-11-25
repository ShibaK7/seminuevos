#include "domain/finance/rules/financeaccess.h"

namespace domain {

FinanceAccess::FinanceAccess(UserRole role)
    : m_role(role)
{
}

bool FinanceAccess::canUseFinance() const
{
    return m_role == UserRole::Admin;
}

} // namespace domain
