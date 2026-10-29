#ifndef PRESENTATION_COMMERCIAL_SALEDETAILS_ISALEDETAILSVIEW_H
#define PRESENTATION_COMMERCIAL_SALEDETAILS_ISALEDETAILSVIEW_H

#include "application/commercial/dto/commercialdtos.h"

#include <QString>

namespace presentation {

class ICommercialSaleDetailsView
{
public:
    virtual ~ICommercialSaleDetailsView() = default;

    virtual void showSaleDetails(const application::CommercialSaleDetailsDto &details) = 0;
    virtual void showSaleDetailsError(const QString &message) = 0;

protected:
    ICommercialSaleDetailsView() = default;
    ICommercialSaleDetailsView(const ICommercialSaleDetailsView &) = default;
    ICommercialSaleDetailsView &operator=(const ICommercialSaleDetailsView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_SALEDETAILS_ISALEDETAILSVIEW_H
