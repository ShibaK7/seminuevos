#ifndef PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSPRESENTER_H
#define PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/inventory/acquisition/acquisitiontermspresenter.h"
#include "presentation/inventory/registration/wizardsteppresenter.h"

#include <optional>

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleDetailsView;

// Paso 1: datos del vehículo, contraparte y operación. Valida el paso y
// compone al presenter de la sección de Adquisición, que es dueño de la
// factura adjunta y de la regla de la autofactura: este solo le avisa cuando
// cambia el tipo de factura.
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
    // Lo que hay en cada cuenta de dinero, releído (otra sesión pudo moverlo).
    void setCashAvailable(const QMap<domain::CashAccount, qint64> &available);

    // Lo capturado más la factura que cuenta como adjunta.
    application::VehicleDetailsDto dto() const;

    // Pinta una unidad ya guardada (el expediente), factura incluida, de
    // modo que dto() devuelva lo mismo. Va después de setLookups().
    void showStored(int folio, const application::VehicleDetailsDto &details);
    void showMaintenanceCost(double cost);

    void onInvoiceTypeChanged();

    // La sección de Adquisición: la vista le conecta los botones de la factura.
    AcquisitionTermsPresenter &acquisitionTerms();

protected:
    domain::ValidationResult collectErrors() const override;

private:
    bool isAutofacturaSelected() const;

    IVehicleDetailsView &m_view;
    const application::VehicleRegistrationService &m_service;
    AcquisitionTermsPresenter m_acquisitionTerms;
    // UMA vigente y tope de efectivo, leídos con los catálogos. Sin UMA no se
    // valida el tope y el pago en efectivo se rechaza.
    domain::CashLimitTerms m_cashLimit;
    // Lo que hay en Caja Chica y Cantidad Banco: una compra de contado no
    // puede pasar de lo que hay en la cuenta de su método. Vacío: no se revisa.
    QMap<domain::CashAccount, qint64> m_cashAvailable;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSPRESENTER_H
