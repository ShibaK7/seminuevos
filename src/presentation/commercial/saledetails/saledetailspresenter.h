#ifndef PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILSPRESENTER_H
#define PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILSPRESENTER_H

#include "application/commercial/services/commercialservice.h"

#include <QObject>

namespace presentation {
class ICommercialSaleDetailsView;
class TaskRunner;

class CommercialSaleDetailsPresenter final : public QObject
{
public:
    CommercialSaleDetailsPresenter(ICommercialSaleDetailsView &view,
                                  const application::CommercialService &service,
                                  TaskRunner &runner, int saleFolio, domain::UserRole role,
                                  QObject *parent = nullptr);

    void load();
    void loadFullDetails();
    void setPaymentWaived(int paymentFolio, bool waived);

private:
    ICommercialSaleDetailsView &m_view;
    const application::CommercialService &m_service;
    TaskRunner &m_runner;
    const int m_saleFolio;
    const domain::UserRole m_role;
    application::CommercialSaleDetailsDto m_currentDetails;
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILSPRESENTER_H
