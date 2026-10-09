#include "presentation/inventory/acquisition/acquisitiontermspresenter.h"

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/inventory/acquisition/iacquisitiontermsview.h"

namespace presentation {

AcquisitionTermsPresenter::AcquisitionTermsPresenter(
    IAcquisitionTermsView &view, const application::VehicleRegistrationService &service)
    : m_view(view)
    , m_service(service)
{
}

void AcquisitionTermsPresenter::setAutofactura(bool autofactura)
{
    m_attachment.setAutofactura(autofactura);
    refresh();
}

void AcquisitionTermsPresenter::onBrowseInvoice()
{
    const QString path = m_view.askInvoiceFile();
    if (path.isEmpty())
        return;
    m_attachment.attach(path);
    refresh();
}

void AcquisitionTermsPresenter::onCfdiRequest()
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
    refresh();
}

QString AcquisitionTermsPresenter::attachedInvoice() const
{
    return m_attachment.attachedPath();
}

void AcquisitionTermsPresenter::refresh()
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
