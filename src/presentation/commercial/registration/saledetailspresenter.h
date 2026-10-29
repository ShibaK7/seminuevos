#ifndef PRESENTATION_COMMERCIAL_REGISTRATION_SALEDETAILSPRESENTER_H
#define PRESENTATION_COMMERCIAL_REGISTRATION_SALEDETAILSPRESENTER_H

#include "application/common/dto/counterpartydtos.h"
#include "application/commercial/sales/dto/saledtos.h"

#include <QObject>
#include <atomic>
#include <memory>

namespace application {
class CounterpartyLookupService;
class SaleService;
}

namespace presentation {

class ISaleDetailsView;
class TaskRunner;

class SaleDetailsPresenter final : public QObject
{
    Q_OBJECT

public:
    SaleDetailsPresenter(ISaleDetailsView &view,
                         const application::CounterpartyLookupService &counterparties,
                         TaskRunner &runner, QObject *parent = nullptr,
                         const application::SaleService *vehicles = nullptr);

public slots:
    void searchCounterparty(const QString &query, bool guarantor);
    void searchVehicle(const QString &query);
    void searchVendors(const QString &query);
    void registerSale(const application::SaleRegistrationDto &sale);

signals:
    void saleRegistrationStarted();
    void saleRegistrationFinished(bool success, int saleFolio, const QString &message);

private:
    ISaleDetailsView &m_view;
    const application::CounterpartyLookupService &m_counterparties;
    const application::SaleService *m_vehicles = nullptr;
    TaskRunner &m_runner;
    std::shared_ptr<std::atomic<int>> m_buyerGeneration = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<int>> m_guarantorGeneration = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<int>> m_vehicleGeneration = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<int>> m_vendorGeneration = std::make_shared<std::atomic<int>>(0);
    bool m_registrationBusy = false;
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_REGISTRATION_SALEDETAILSPRESENTER_H
