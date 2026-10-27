#ifndef PRESENTATION_COMMERCIAL_SALEDETAILS_ISALEDETAILSVIEW_H
#define PRESENTATION_COMMERCIAL_SALEDETAILS_ISALEDETAILSVIEW_H

#include "application/commercial/dto/commercialdtos.h"

namespace presentation {

class ISaleDetailsView
{
public:
    virtual ~ISaleDetailsView() = default;

    virtual void showSaleDetails(const application::CommercialSaleDetailsDto &details) = 0;
    virtual void showSaleDetailsError(const QString &message) = 0;

protected:
    ISaleDetailsView() = default;
    ISaleDetailsView(const ISaleDetailsView &) = default;
    ISaleDetailsView &operator=(const ISaleDetailsView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_SALEDETAILS_ISALEDETAILSVIEW_H
