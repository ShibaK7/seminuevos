#ifndef PRESENTATION_COMMERCIAL_ICOMMERCIALVIEW_H
#define PRESENTATION_COMMERCIAL_ICOMMERCIALVIEW_H

#include "application/commercial/dto/commercialdtos.h"

namespace presentation {

class ICommercialView
{
public:
    virtual ~ICommercialView() = default;

    virtual application::CommercialFilterDto filter() const = 0;
    virtual void showSales(const QList<application::CommercialSaleDto> &sales) = 0;
    virtual void showCommercialMessage(const QString &message) = 0;

protected:
    ICommercialView() = default;
    ICommercialView(const ICommercialView &) = default;
    ICommercialView &operator=(const ICommercialView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_ICOMMERCIALVIEW_H
