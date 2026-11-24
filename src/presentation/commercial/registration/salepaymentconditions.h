#ifndef PRESENTATION_COMMERCIAL_REGISTRATION_SALEPAYMENTCONDITIONS_H
#define PRESENTATION_COMMERCIAL_REGISTRATION_SALEPAYMENTCONDITIONS_H

#include "application/commercial/sales/dto/saledtos.h"

#include <QString>

namespace presentation {

QString salePaymentConditions(const application::SaleRegistrationDto &sale);

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_REGISTRATION_SALEPAYMENTCONDITIONS_H
