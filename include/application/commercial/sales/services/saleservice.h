#ifndef APPLICATION_COMMERCIAL_SALES_SERVICES_SALESERVICE_H
#define APPLICATION_COMMERCIAL_SALES_SERVICES_SALESERVICE_H

#include "application/common/ports/counterpartydirectory.h"
#include "application/commercial/sales/ports/salerepository.h"
#include "domain/common/value_objects/validationresult.h"

namespace application {

// Sale use case boundary. Financing values are accepted as an explicit
// snapshot; this service intentionally contains no tax, interest, or schedule
// calculations until the business policy is confirmed.
class SaleService final
{
public:
    SaleService(SaleRepository &sales, CounterpartyDirectory &counterparties);

    SaleSearchResultDto searchAvailableVehicles(const QString &text) const;
    SaleVendorSearchResultDto searchVendors(const QString &text) const;
    SaleCounterpartySearchResultDto searchCounterparties(const QString &text) const;
    CounterpartyUpdateResultDto updateCounterparty(const CounterpartyDto &counterparty) const;
    domain::ValidationResult validateSale(const SaleRegistrationDto &sale) const;
    SaleRegistrationResultDto registerSale(const SaleRegistrationDto &sale) const;

private:
    SaleRepository &m_sales;
    CounterpartyDirectory &m_counterparties;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_SALES_SERVICES_SALESERVICE_H
