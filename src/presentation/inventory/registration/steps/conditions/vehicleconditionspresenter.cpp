#include "presentation/inventory/registration/steps/conditions/vehicleconditionspresenter.h"

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/inventory/registration/steps/conditions/ivehicleconditionsview.h"

namespace presentation {

VehicleConditionsPresenter::VehicleConditionsPresenter(
    IVehicleConditionsView &view, const application::VehicleRegistrationService &service)
    : WizardStepPresenter(view)
    , m_view(view)
    , m_service(service)
{
}

QString VehicleConditionsPresenter::title() const
{
    return QStringLiteral("Condición");
}

bool VehicleConditionsPresenter::ownsField(const QString &field) const
{
    return field.startsWith(QStringLiteral("conditions"))
           || field.startsWith(QStringLiteral("inspection"));
}

void VehicleConditionsPresenter::setLookups(const application::RegistrationLookupsDto &lookups)
{
    m_view.setFuelTypes(lookups.fuelTypes);
    if (lookups.checklist.isEmpty()) {
        m_view.showChecklistMessage(QStringLiteral(
            "El catálogo de condiciones (vehicle_conditions_cat) está vacío o no se pudo "
            "leer, así que el checklist no está disponible."));
        return;
    }
    m_view.setChecklist(lookups.checklist);
}

application::VehicleConditionsDto VehicleConditionsPresenter::dto() const
{
    return m_view.conditions();
}

domain::ValidationResult VehicleConditionsPresenter::collectErrors() const
{
    return m_service.validateConditions(dto());
}

} // namespace presentation
