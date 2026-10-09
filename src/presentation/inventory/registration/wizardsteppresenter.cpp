#include "presentation/inventory/registration/wizardsteppresenter.h"
#include "presentation/inventory/registration/istepview.h"

#include <algorithm>

namespace presentation {

WizardStepPresenter::WizardStepPresenter(IStepView &view)
    : m_view(view)
{
}

domain::ValidationResult WizardStepPresenter::validate() const
{
    return ownErrors(collectErrors());
}

domain::ValidationResult WizardStepPresenter::showErrors(const domain::ValidationResult &result)
{
    const QList<domain::ValidationError> unshownOwn =
        m_view.showFieldErrors(ownErrors(result).errors());

    // En el orden de result, para que el aviso los liste como los reportó el
    // dominio.
    domain::ValidationResult unshown;
    for (const domain::ValidationError &error : result.errors()) {
        const bool shown = ownsField(error.field)
                           && std::none_of(unshownOwn.cbegin(), unshownOwn.cend(),
                                           [&error](const domain::ValidationError &other) {
                                               return other.field == error.field
                                                      && other.message == error.message;
                                           });
        if (!shown)
            unshown.addError(error.field, error.message);
    }
    return unshown;
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
