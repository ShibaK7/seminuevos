#ifndef PRESENTATION_INVENTORY_ACQUISITION_ACQUISITIONTERMSPRESENTER_H
#define PRESENTATION_INVENTORY_ACQUISITION_ACQUISITIONTERMSPRESENTER_H

#include "presentation/inventory/acquisition/invoiceattachment.h"

#include <QString>

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IAcquisitionTermsView;

// Lo que solo existe en una compra, dentro del Paso 1: la factura adjunta y la
// regla de la autofactura (InvoiceAttachment). La sección solo pinta lo que
// este presenter decide; el presenter del paso lo compone y le avisa cuando
// cambia el tipo de factura.
class AcquisitionTermsPresenter final
{
public:
    AcquisitionTermsPresenter(IAcquisitionTermsView &view,
                              const application::VehicleRegistrationService &service);

    // Cambió el tipo de factura (también al empezar): aparta o recupera la
    // factura según la regla de la autofactura y vuelve a pintar la fila.
    void setAutofactura(bool autofactura);

    void onBrowseInvoice();
    void onCfdiRequest();

    // La factura que cuenta como adjunta; vacía si no hay o si está apartada.
    QString attachedInvoice() const;

private:
    void refresh();

    IAcquisitionTermsView &m_view;
    const application::VehicleRegistrationService &m_service;
    InvoiceAttachment m_attachment;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_ACQUISITION_ACQUISITIONTERMSPRESENTER_H
