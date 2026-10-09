#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/inventory/registration/steps/details/ivehicledetailsview.h"

namespace presentation {

VehicleDetailsPresenter::VehicleDetailsPresenter(
    IVehicleDetailsView &view, const application::VehicleRegistrationService &service)
    : WizardStepPresenter(view)
    , m_view(view)
    , m_service(service)
    , m_acquisitionTerms(view.acquisitionTerms(), service)
{
}

QString VehicleDetailsPresenter::title() const
{
    return QStringLiteral("Detalles");
}

bool VehicleDetailsPresenter::ownsField(const QString &field) const
{
    return !field.startsWith(QStringLiteral("conditions"))
           && !field.startsWith(QStringLiteral("inspection"))
           && !field.startsWith(QStringLiteral("images"))
           && !field.startsWith(QStringLiteral("documents"));
}

void VehicleDetailsPresenter::start()
{
    m_acquisitionTerms.setAutofactura(isAutofacturaSelected());
}

void VehicleDetailsPresenter::setLookups(const application::RegistrationLookupsDto &lookups)
{
    m_umaDailyValue = lookups.umaDailyValue;
    m_view.setLookups(lookups);
}

application::VehicleDetailsDto VehicleDetailsPresenter::dto() const
{
    application::VehicleDetailsDto details = m_view.details();
    details.invoiceFilePath = m_acquisitionTerms.attachedInvoice();
    return details;
}

domain::ValidationResult VehicleDetailsPresenter::collectErrors() const
{
    return m_service.validateDetails(dto(), m_umaDailyValue);
}

bool VehicleDetailsPresenter::isAutofacturaSelected() const
{
    // Un combo vacío (la vista lo vacía un instante al cambiar de rama) no es
    // Autofactura: así cambiar de rama recupera la factura apartada, igual que
    // cualquier factura sobrevive a ir y volver por la consignación.
    const std::optional<domain::InvoiceType> type = m_view.details().invoiceType;
    return type && *type == domain::InvoiceType::Autofactura;
}

void VehicleDetailsPresenter::onInvoiceTypeChanged()
{
    m_acquisitionTerms.setAutofactura(isAutofacturaSelected());
}

AcquisitionTermsPresenter &VehicleDetailsPresenter::acquisitionTerms()
{
    return m_acquisitionTerms;
}

} // namespace presentation
