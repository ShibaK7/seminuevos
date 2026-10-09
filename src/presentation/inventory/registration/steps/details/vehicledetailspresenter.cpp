#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/inventory/registration/steps/details/ivehicledetailsview.h"

namespace presentation {

VehicleDetailsPresenter::VehicleDetailsPresenter(
    IVehicleDetailsView &view, const application::VehicleRegistrationService &service)
    : WizardStepPresenter(view)
    , m_view(view)
    , m_service(service)
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
    m_attachment.setAutofactura(isAutofacturaSelected());
    refreshInvoiceAttachment();
}

void VehicleDetailsPresenter::setLookups(const application::RegistrationLookupsDto &lookups)
{
    m_umaDailyValue = lookups.umaDailyValue;
    m_view.setLookups(lookups);
}

application::VehicleDetailsDto VehicleDetailsPresenter::dto() const
{
    application::VehicleDetailsDto details = m_view.details();
    details.invoiceFilePath = m_attachment.attachedPath();
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
    m_attachment.setAutofactura(isAutofacturaSelected());
    refreshInvoiceAttachment();
}

void VehicleDetailsPresenter::onBrowseInvoice()
{
    const QString path = m_view.askInvoiceFile();
    if (path.isEmpty())
        return;
    m_attachment.attach(path);
    refreshInvoiceAttachment();
}

void VehicleDetailsPresenter::onCfdiRequest()
{
    const application::TemporaryFileDto form = m_service.prepareCfdiRequestForm();
    if (!form.ok) {
        m_view.showWarning(QStringLiteral("No se pudo abrir el documento"), form.errorMessage);
        return;
    }
    if (!m_view.openDocument(form.path)) {
        m_view.showWarning(QStringLiteral("No se pudo abrir el documento"),
                           QStringLiteral("No se pudo abrir el navegador para mostrar:\n%1").arg(form.path));
        return;
    }

    // Solo cuenta como generada cuando todo salió bien: ni se desbloquea la
    // subida con una solicitud que el usuario nunca llegó a ver, ni se pierde
    // una factura que todavía puede recuperar volviendo a Facturado.
    m_attachment.markCfdiRequestGenerated();
    refreshInvoiceAttachment();
}

void VehicleDetailsPresenter::refreshInvoiceAttachment()
{
    InvoiceAttachmentState state;
    state.fileLabel = m_attachment.label();
    state.fileToolTip = m_attachment.labelToolTip();
    state.cfdiButtonVisible = m_attachment.showsCfdiButton();
    state.uploadEnabled = m_attachment.canUpload();
    state.uploadToolTip = m_attachment.uploadToolTip();
    m_view.showInvoiceAttachment(state);
}

} // namespace presentation
