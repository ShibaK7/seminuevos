#include "presentation/commercial/saledetails/saledetailspresenter.h"

#include "presentation/commercial/saledetails/isaledetailsview.h"
#include "presentation/common/tasks/taskrunner.h"

namespace presentation {

CommercialSaleDetailsPresenter::CommercialSaleDetailsPresenter(ICommercialSaleDetailsView &view,
                                                             const application::CommercialService &service,
                                                             TaskRunner &runner, int saleFolio,
                                                             domain::UserRole role, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
    , m_saleFolio(saleFolio)
    , m_role(role)
{
}

void CommercialSaleDetailsPresenter::load()
{
    const application::CommercialService *service = &m_service;
    const int folio = m_saleFolio;
    m_runner.run(this, [service, folio] { return service->saleDetails(folio); },
                 [this](const application::CommercialSaleDetailsResultDto &result) {
                     if (!result.errorMessage.isEmpty())
                         m_view.showSaleDetailsError(result.errorMessage);
                     else {
                         m_currentDetails = result.sale;
                         m_view.showSaleDetails(result.sale);
                     }
                 });
}

void CommercialSaleDetailsPresenter::setPaymentWaived(int paymentFolio, bool waived)
{
    const application::CommercialService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(this, [service, paymentFolio, waived, role] {
                     return service->updatePaymentWaiver(paymentFolio, waived, role);
                 },
                 [this](const application::CommercialWriteResultDto &result) {
                     if (result.ok) {
                         load();
                     } else {
                         m_view.showSaleDetails(m_currentDetails);
                         m_view.showSaleDetailsError(result.errorMessage);
                     }
                 });
}

} // namespace presentation
