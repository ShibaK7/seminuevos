#ifndef ADAPTERS_PERSISTENCE_COMMERCIAL_SQLCOMMERCIAL_H
#define ADAPTERS_PERSISTENCE_COMMERCIAL_SQLCOMMERCIAL_H

#include "application/commercial/ports/commercialreader.h"
#include "application/commercial/ports/commercialpaymentwriter.h"

class ConnectionPool;

class SqlCommercial final : public application::CommercialReader,
                            public application::CommercialPaymentWriter
{
public:
    explicit SqlCommercial(ConnectionPool &pool);

    QList<application::CommercialSaleDto> search(
        const application::CommercialFilterDto &filter, QString *error) override;
    application::CommercialSaleDetailsDto saleDetails(int folio, QString *error) override;
    bool updatePaymentWaiver(int paymentFolio, bool waived, domain::UserRole role,
                             QString *error) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_COMMERCIAL_SQLCOMMERCIAL_H
