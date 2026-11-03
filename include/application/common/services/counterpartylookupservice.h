#ifndef APPLICATION_COMMON_SERVICES_COUNTERPARTYLOOKUPSERVICE_H
#define APPLICATION_COMMON_SERVICES_COUNTERPARTYLOOKUPSERVICE_H

#include "application/common/dto/counterpartydtos.h"
#include "application/common/ports/counterpartydirectory.h"

namespace application {

// Shared lookup use case for existing people. Database access is delegated to
// the directory port so presentation never depends on a SQL adapter.
class CounterpartyLookupService final
{
public:
    explicit CounterpartyLookupService(CounterpartyDirectory &counterparties);

    CounterpartySearchResultDto search(const QString &query) const;

private:
    CounterpartyDirectory &m_counterparties;
};

} // namespace application

#endif // APPLICATION_COMMON_SERVICES_COUNTERPARTYLOOKUPSERVICE_H
