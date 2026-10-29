#ifndef APPLICATION_COMMERCIAL_PORTS_COMMERCIALREADER_H
#define APPLICATION_COMMERCIAL_PORTS_COMMERCIALREADER_H

#include "application/commercial/dto/commercialdtos.h"

namespace application {

class CommercialReader
{
public:
    virtual ~CommercialReader() = default;

    virtual QList<CommercialSaleDto> search(const CommercialFilterDto &filter,
                                            QString *error) = 0;
    virtual CommercialSaleDetailsDto saleDetails(int folio, QString *error) = 0;
    virtual CommercialSaleRegistrationDetailsDto saleRegistrationDetails(int folio,
                                                                          QString *error) = 0;

protected:
    CommercialReader() = default;
    CommercialReader(const CommercialReader &) = default;
    CommercialReader &operator=(const CommercialReader &) = default;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_PORTS_COMMERCIALREADER_H
