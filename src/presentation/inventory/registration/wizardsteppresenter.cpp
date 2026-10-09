#include "presentation/inventory/registration/wizardsteppresenter.h"
#include "presentation/inventory/registration/istepview.h"

namespace presentation {

WizardStepPresenter::WizardStepPresenter(IStepView &view)
    : m_view(view)
{
}

domain::ValidationResult WizardStepPresenter::validate() const
{
    return ownErrors(collectErrors());
}

void WizardStepPresenter::showErrors(const domain::ValidationResult &result)
{
    m_view.showFieldErrors(ownErrors(result).errors());
}

bool WizardStepPresenter::focusFirstError(const domain::ValidationResult &result)
{
    // El primero que tenga dónde mostrarse: uno que no se captura en
    // pantalla, como la UMA, no debe dejar al usuario sin cursor.
    for (const domain::ValidationError &error : ownErrors(result).errors()) {
        if (m_view.focusField(error.field))
            return true;
    }
    return false;
}

domain::ValidationResult WizardStepPresenter::ownErrors(const domain::ValidationResult &result) const
{
    domain::ValidationResult own;
    for (const domain::ValidationError &error : result.errors()) {
        if (ownsField(error.field))
            own.addError(error.field, error.message);
    }
    return own;
}

} // namespace presentation
