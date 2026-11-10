#ifndef APPLICATION_COMMERCIAL_SALES_PORTS_SALEREPOSITORY_H
#define APPLICATION_COMMERCIAL_SALES_PORTS_SALEREPOSITORY_H

#include "application/commercial/sales/dto/saledtos.h"

namespace application {

// Read/write boundary for the commercial sale aggregate. Implementations own
// SQL details and must save the sale, financing schedule, and vehicle status
// change atomically.
class SaleRepository
{
public:
    virtual ~SaleRepository() = default;

    virtual QList<SaleVehicleOptionDto> searchAvailableVehicles(const QString &text,
                                                                 QString *error) = 0;
    virtual QList<SaleVendorOptionDto> searchVendors(const QString &text,
                                                     QString *error) = 0;
    virtual SaleRegistrationResultDto registerSale(const SaleRegistrationDto &sale) = 0;

protected:
    SaleRepository() = default;
    SaleRepository(const SaleRepository &) = default;
    SaleRepository &operator=(const SaleRepository &) = default;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_SALES_PORTS_SALEREPOSITORY_H
