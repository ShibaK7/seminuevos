#ifndef PRESENTATION_INVENTORY_REGISTRATION_VEHICLEFORMPRESENTER_H
#define PRESENTATION_INVENTORY_REGISTRATION_VEHICLEFORMPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "domain/common/value_objects/validationresult.h"
#include "presentation/common/navigation/wizardnavigator.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <array>
#include <memory>

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleConditionsView;
class IVehicleDetailsView;
class IVehicleFilesView;
class IVehicleFormView;
class VehicleConditionsPresenter;
class VehicleDetailsPresenter;
class VehicleFilesPresenter;
class WizardStepPresenter;

// Lo común del formulario de un vehículo por pasos, que se usa para darlo de
// alta (VehicleWizardPresenter) y para consultarlo y corregirlo
// (VehicleRecordPresenter). Clase abstracta: tiene los tres pasos con sus
// presenters, los catálogos, el ruteo de los errores de validación a su paso
// y a su campo, la validación de los tres pasos al guardar y la revalidación
// en vivo con debounce.
//
// onStepEdited() y revalidateEditedSteps() son el Template Method: cada
// subclase solo dice cuándo cuenta una edición, qué más hacer con ella y cómo
// se pinta su stepper (candados y palomitas en el alta, pestañas en el
// expediente).
//
// Sin widgets (solo QtCore): se prueba con vistas falsas.
class VehicleFormPresenter : public QObject
{
    Q_OBJECT

public:
    ~VehicleFormPresenter() override;

    VehicleDetailsPresenter &details();
    VehicleFilesPresenter &files();

    // Algo cambió en el paso `step`. Si la edición cuenta, no revalida en el
    // acto: arranca (o reinicia) un debounce, y la revalidación corre cuando
    // el usuario deja de teclear.
    void onStepEdited(int step);
    // Corre ya la revalidación pendiente, sin esperar el debounce.
    void revalidateEditedSteps();

protected:
    static constexpr int kStepCount = 3;

    VehicleFormPresenter(IVehicleFormView &form, IVehicleDetailsView &details,
                         IVehicleConditionsView &conditions, IVehicleFilesView &files,
                         const application::VehicleRegistrationService &service,
                         bool freeNavigation, QObject *parent);

    // --- Ganchos ---

    // Si una edición cuenta ahora. Mientras se pintan catálogos o datos, los
    // campos también avisan cambios que no son del usuario.
    virtual bool editCounts() const = 0;
    // Qué más hacer con una edición que cuenta, antes de revalidar.
    virtual void noteEdit();
    // Cómo se pinta el stepper.
    virtual void refreshStepper() = 0;
    // El título del aviso al llevar al usuario a un paso inválido;
    // alreadyVisible dice si ya estaba en él.
    virtual QString invalidStepTitle(int step, bool alreadyVisible) const;
    // Muestra el paso actual y su stepper.
    virtual void showCurrentStep();

    // --- Lo común ---

    void applyLookups(const application::RegistrationLookupsDto &lookups);
    // Valida los tres pasos, como al guardar: los marca como intentados y
    // cada uno marca sus campos, también los que no se ven (al ir a ellos ya
    // dicen qué falta). Devuelve el primero inválido, o -1, y su resultado en
    // firstInvalid.
    int validateAllSteps(domain::ValidationResult &firstInvalid);
    // El primer paso, en el orden del formulario, que tenga alguno de esos
    // errores: el usuario corrige en ese orden, sin importar en cuál los
    // reportó el dominio.
    int stepOwning(const domain::ValidationResult &result) const;
    // Lleva al usuario al paso `step`, que no pasó la validación, lo explica
    // en el aviso y le da el foco al primer campo que falla. Solo para
    // intentos explícitos (avanzar o guardar).
    void showInvalidStep(int step, const domain::ValidationResult &result);
    // Marca en el paso los campos que fallaron y limpia los demás. Devuelve
    // los errores que no quedaron a la vista bajo ningún campo.
    domain::ValidationResult markStepFields(int step, const domain::ValidationResult &result);
    // El aviso bajo las páginas: el título y, si hay, los errores que no se
    // ven bajo ningún campo.
    void showValidationErrors(const QString &title, const domain::ValidationResult &unshown);
    // Olvida la revalidación pendiente.
    void cancelRevalidation();

    IVehicleFormView &m_form;
    std::unique_ptr<VehicleDetailsPresenter> m_details;
    std::unique_ptr<VehicleConditionsPresenter> m_conditions;
    std::unique_ptr<VehicleFilesPresenter> m_files;
    // Los tres en el orden en que se muestran.
    std::array<WizardStepPresenter *, kStepCount> m_steps{};
    WizardNavigator m_nav;

private:
    // Debounce de la revalidación en vivo: un solo temporizador y los pasos
    // que se editaron mientras corría.
    QTimer m_revalidateTimer;
    QSet<int> m_editedSteps;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_VEHICLEFORMPRESENTER_H
