#ifndef PRESENTATION_INVENTORY_REGISTRATION_WIZARDSTEPPRESENTER_H
#define PRESENTATION_INVENTORY_REGISTRATION_WIZARDSTEPPRESENTER_H

#include "domain/common/value_objects/validationresult.h"

#include <QString>

namespace presentation {

class IStepView;

// Un paso del asistente de registro. Clase abstracta: cada paso dice su
// título, qué campos son suyos y cómo se valida; lo común (quedarse solo con
// los errores propios, pintarlos y dar el foco) vive aquí una sola vez.
//
// validate(), showErrors() y focusFirstError() NO son virtuales: son el
// Template Method. Cada subclase solo llena collectErrors() y ownsField().
class WizardStepPresenter
{
public:
    virtual ~WizardStepPresenter() = default;

    WizardStepPresenter(const WizardStepPresenter &) = delete;
    WizardStepPresenter &operator=(const WizardStepPresenter &) = delete;

    virtual QString title() const = 0;
    // Si un error con esta clave le toca a este paso. Con esto se rutean los
    // rechazos del guardado, que llegan de los tres pasos juntos.
    virtual bool ownsField(const QString &field) const = 0;

    // Lo que el paso tiene capturado ahora, según el dominio. Solo errores de
    // este paso. Es pura (sin E/S).
    domain::ValidationResult validate() const;

    // Marca en la vista los campos de este paso que fallan y limpia los demás.
    // Devuelve los errores de result que no quedaron a la vista bajo ningún
    // campo: los de otros pasos y los de este que no tienen campo en
    // pantalla.
    domain::ValidationResult showErrors(const domain::ValidationResult &result);
    // Le da el foco al primer error de este paso que tenga un campo en
    // pantalla. Devuelve si encontró uno.
    bool focusFirstError(const domain::ValidationResult &result);

protected:
    explicit WizardStepPresenter(IStepView &view);

    virtual domain::ValidationResult collectErrors() const = 0;

private:
    domain::ValidationResult ownErrors(const domain::ValidationResult &result) const;

    IStepView &m_view;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_WIZARDSTEPPRESENTER_H
