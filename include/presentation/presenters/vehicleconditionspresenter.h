#ifndef PRESENTATION_PRESENTERS_VEHICLECONDITIONSPRESENTER_H
#define PRESENTATION_PRESENTERS_VEHICLECONDITIONSPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/presenters/wizardsteppresenter.h"

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleConditionsView;

// Paso 2: especificaciones y checklist de condición.
class VehicleConditionsPresenter final : public WizardStepPresenter
{
public:
    VehicleConditionsPresenter(IVehicleConditionsView &view,
                               const application::VehicleRegistrationService &service);

    QString title() const override;
    // "conditions.*" y los renglones de la inspección.
    bool ownsField(const QString &field) const override;

    void setLookups(const application::RegistrationLookupsDto &lookups);

    application::VehicleConditionsDto dto() const;

protected:
    domain::ValidationResult collectErrors() const override;

private:
    IVehicleConditionsView &m_view;
    const application::VehicleRegistrationService &m_service;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_VEHICLECONDITIONSPRESENTER_H
