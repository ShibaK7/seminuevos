#ifndef ADAPTERS_PERSISTENCE_COMMERCIAL_SQLSALEREPOSITORY_H
#define ADAPTERS_PERSISTENCE_COMMERCIAL_SQLSALEREPOSITORY_H

#include "application/commercial/sales/ports/salerepository.h"

class ConnectionPool;

class SqlSaleRepository final : public application::SaleRepository
{
public:
    explicit SqlSaleRepository(ConnectionPool &pool);

    QList<application::SaleVehicleOptionDto> searchAvailableVehicles(const QString &text,
                                                                     QString *error) override;
    QList<application::SaleVendorOptionDto> searchVendors(const QString &text,
                                                         QString *error) override;
    application::SaleRegistrationResultDto registerSale(
        const application::SaleRegistrationDto &sale) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_COMMERCIAL_SQLSALEREPOSITORY_H
