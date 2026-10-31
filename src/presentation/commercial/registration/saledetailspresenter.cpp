#include "presentation/commercial/registration/saledetailspresenter.h"

#include "application/common/services/counterpartylookupservice.h"
#include "application/commercial/sales/services/saleservice.h"
#include "presentation/commercial/registration/isaledetailsview.h"
#include "presentation/common/tasks/taskrunner.h"

namespace presentation {

SaleDetailsPresenter::SaleDetailsPresenter(
    ISaleDetailsView &view, const application::CounterpartyLookupService &counterparties,
    TaskRunner &runner, QObject *parent, const application::SaleService *vehicles)
    : QObject(parent)
    , m_view(view)
    , m_counterparties(counterparties)
    , m_vehicles(vehicles)
    , m_runner(runner)
{
}

void SaleDetailsPresenter::searchCounterparty(const QString &query, bool guarantor)
{
    std::shared_ptr<std::atomic<int>> generation =
        guarantor ? m_guarantorGeneration : m_buyerGeneration;
    const int request = ++(*generation);
    const QString text = query.trimmed();
    const application::CounterpartyLookupService *service = &m_counterparties;
    const std::shared_ptr<std::atomic<int>> latest = generation;

    m_runner.run(this,
                 [service, text, request, latest] {
                     if (latest->load() != request)
                         return application::CounterpartySearchResultDto();
                     return service->search(text);
                 },
                 [this, guarantor, request, latest](const application::CounterpartySearchResultDto &result) {
                     if (latest->load() != request)
                         return;
                     m_view.setCounterpartyMatches(guarantor, result.counterparties);
                     m_view.showCounterpartySearchError(guarantor, result.errorMessage);
                 });
}

void SaleDetailsPresenter::searchVehicle(const QString &query)
{
    if (!m_vehicles) {
        m_view.showVehicleSearchError(QStringLiteral("La búsqueda de vehículos no está conectada al servicio."));
        return;
    }

    const int request = ++(*m_vehicleGeneration);
    const QString text = query.trimmed();
    const application::SaleService *service = m_vehicles;
    const std::shared_ptr<std::atomic<int>> latest = m_vehicleGeneration;

    m_runner.run(this,
                 [service, text, request, latest] {
                     if (latest->load() != request)
                         return application::SaleSearchResultDto();
                     return service->searchAvailableVehicles(text);
                 },
                 [this, request, latest](const application::SaleSearchResultDto &result) {
                     if (latest->load() != request)
                         return;
                     m_view.setVehicleMatches(result.vehicles);
                     m_view.showVehicleSearchError(result.errorMessage);
                 });
}

void SaleDetailsPresenter::searchVendors(const QString &query)
{
    if (!m_vehicles) {
        m_view.showVendorSearchError(QStringLiteral("La búsqueda de vendedores no está conectada al servicio."));
        return;
    }

    const int request = ++(*m_vendorGeneration);
    const QString text = query.trimmed();
    const application::SaleService *service = m_vehicles;
    const std::shared_ptr<std::atomic<int>> latest = m_vendorGeneration;

    m_runner.run(this,
                 [service, text, request, latest] {
                     if (latest->load() != request)
                         return application::SaleVendorSearchResultDto();
                     return service->searchVendors(text);
                 },
                 [this, request, latest](const application::SaleVendorSearchResultDto &result) {
                     if (latest->load() != request)
                         return;
                     m_view.setVendorMatches(result.vendors);
                     m_view.showVendorSearchError(result.errorMessage);
                 });
}

void SaleDetailsPresenter::registerSale(const application::SaleRegistrationDto &sale)
{
    if (!m_vehicles || m_registrationBusy)
        return;

    const domain::ValidationResult validation = m_vehicles->validateSale(sale);
    m_view.showSaleValidationErrors(validation.errors());
    if (!validation.isValid()) {
        for (const domain::ValidationError &error : validation.errors()) {
            if (m_view.focusSaleField(error.field))
                break;
        }
        emit saleRegistrationFinished(false, -1, "Revisa los campos marcados.");
        return;
    }

    m_registrationBusy = true;
    emit saleRegistrationStarted();
    const application::SaleService *service = m_vehicles;
    m_runner.run(this,
                 [service, sale] { return service->registerSale(sale); },
                 [this](const application::SaleRegistrationResultDto &result) {
                     m_registrationBusy = false;
                     emit saleRegistrationFinished(
                         result.status == application::SaleRegistrationResultDto::Status::Registered,
                         result.saleFolio,
                         result.message);
                 });
}

} // namespace presentation
