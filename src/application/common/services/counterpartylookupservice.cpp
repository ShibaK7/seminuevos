#include "application/common/services/counterpartylookupservice.h"

namespace application {

CounterpartyLookupService::CounterpartyLookupService(CounterpartyDirectory &counterparties)
    : m_counterparties(counterparties)
{
}

CounterpartySearchResultDto CounterpartyLookupService::search(const QString &query) const
{
    CounterpartySearchResultDto result;
    const QString normalized = query.trimmed();
    if (normalized.size() < 3)
        return result;

    result.counterparties = m_counterparties.search(normalized, &result.errorMessage);
    return result;
}

} // namespace application
