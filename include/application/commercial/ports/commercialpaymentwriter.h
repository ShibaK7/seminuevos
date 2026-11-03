#ifndef APPLICATION_COMMERCIAL_PORTS_COMMERCIALPAYMENTWRITER_H
#define APPLICATION_COMMERCIAL_PORTS_COMMERCIALPAYMENTWRITER_H

#include "application/commercial/dto/commercialdtos.h"
#include "domain/common/value_objects/enums.h"

namespace application {

class CommercialPaymentWriter
{
public:
    virtual ~CommercialPaymentWriter() = default;

    virtual bool updatePaymentWaiver(int paymentFolio, bool waived, domain::UserRole role,
                                    QString *error) = 0;

protected:
    CommercialPaymentWriter() = default;
    CommercialPaymentWriter(const CommercialPaymentWriter &) = default;
    CommercialPaymentWriter &operator=(const CommercialPaymentWriter &) = default;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_PORTS_COMMERCIALPAYMENTWRITER_H
