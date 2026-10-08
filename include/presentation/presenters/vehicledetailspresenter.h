#ifndef PRESENTATION_PRESENTERS_VEHICLEDETAILSPRESENTER_H
#define PRESENTATION_PRESENTERS_VEHICLEDETAILSPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/presenters/invoiceattachment.h"
#include "presentation/presenters/wizardsteppresenter.h"

#include <optional>

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleDetailsView;

// Paso 1: datos del vehículo, contraparte y operación. Además de validar, es
// dueño de la factura adjunta y de la regla de la autofactura
// (InvoiceAttachment): la vista solo pinta lo que este presenter decide.
class VehicleDetailsPresenter final : public WizardStepPresenter
{
public:
    VehicleDetailsPresenter(IVehicleDetailsView &view,
                            const application::VehicleRegistrationService &service);

    QString title() const override;
    // Todo lo que no es de Condición ni de Archivos: los datos del vehículo,
    // la contraparte ("counterparty.*"), la factura, los precios y la UMA.
    bool ownsField(const QString &field) const override;

    // Pinta el estado inicial de la factura. Se llama una vez, ya conectado.
    void start();
    void setLookups(const application::RegistrationLookupsDto &lookups);

    // Lo capturado más la factura que cuenta como adjunta.
    application::VehicleDetailsDto dto() const;

    void onInvoiceTypeChanged();
    void onBrowseInvoice();
    void onCfdiRequest();

protected:
    domain::ValidationResult collectErrors() const override;

private:
    bool isAutofacturaSelected() const;
    void refreshInvoiceAttachment();

    IVehicleDetailsView &m_view;
    const application::VehicleRegistrationService &m_service;
    InvoiceAttachment m_attachment;
    // UMA vigente, leída con los catálogos. Sin ella no se valida el tope de
    // efectivo y el pago en efectivo se rechaza.
    std::optional<double> m_umaDailyValue;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_VEHICLEDETAILSPRESENTER_H
