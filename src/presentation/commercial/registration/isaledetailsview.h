#ifndef PRESENTATION_COMMERCIAL_REGISTRATION_ISALEDETAILSVIEW_H
#define PRESENTATION_COMMERCIAL_REGISTRATION_ISALEDETAILSVIEW_H

#include "application/common/dto/counterpartydtos.h"
#include "application/commercial/sales/dto/saledtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>

namespace presentation {

class ISaleDetailsView
{
public:
    virtual ~ISaleDetailsView() = default;

    virtual void setCounterpartyMatches(bool guarantor,
                                        const QList<application::CounterpartyDto> &matches) = 0;
    virtual void showCounterpartySearchError(bool guarantor, const QString &message) = 0;
    virtual void setVehicleMatches(const QList<application::SaleVehicleOptionDto> &matches) = 0;
    virtual void showVehicleSearchError(const QString &message) = 0;
    virtual void setVendorMatches(const QList<application::SaleVendorOptionDto> &matches) = 0;
    virtual void showVendorSearchError(const QString &message) = 0;
    virtual QList<domain::ValidationError> showSaleValidationErrors(
        const QList<domain::ValidationError> &errors) = 0;
    virtual bool focusSaleField(const QString &field) = 0;

protected:
    ISaleDetailsView() = default;
    ISaleDetailsView(const ISaleDetailsView &) = default;
    ISaleDetailsView &operator=(const ISaleDetailsView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_REGISTRATION_ISALEDETAILSVIEW_H
