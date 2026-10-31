#ifndef APPLICATION_COMMERCIAL_SERVICES_COMMERCIALSERVICE_H
#define APPLICATION_COMMERCIAL_SERVICES_COMMERCIALSERVICE_H

#include "application/commercial/dto/commercialdtos.h"
#include "application/commercial/ports/commercialpaymentwriter.h"
#include "application/commercial/ports/commercialreader.h"

namespace application {

class CommercialService final
{
public:
    CommercialService(CommercialReader &reader, CommercialPaymentWriter &writer);

    CommercialResultDto search(const CommercialFilterDto &filter) const;
    CommercialSaleDetailsResultDto saleDetails(int folio) const;
    CommercialSaleRegistrationDetailsResultDto saleRegistrationDetails(int folio) const;
    CommercialWriteResultDto updatePaymentWaiver(int paymentFolio, bool waived,
                                                  domain::UserRole role) const;

private:
    CommercialReader &m_reader;
    CommercialPaymentWriter &m_writer;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_SERVICES_COMMERCIALSERVICE_H
