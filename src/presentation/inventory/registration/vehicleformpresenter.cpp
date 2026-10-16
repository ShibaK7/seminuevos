#include "presentation/inventory/registration/vehicleformpresenter.h"

#include "presentation/inventory/registration/ivehicleformview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionspresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"

#include <utility>

namespace presentation {

namespace {

// Cuánto se espera después de la última edición para revalidar. Lo bastante
// corto para que las marcas y el aviso sigan al usuario mientras escribe, y
// lo bastante largo para no revalidar con cada tecla: un mismo cambio además
// avisa varias veces (un spinbox y el QLineEdit que lleva dentro, por ejemplo).
constexpr int kRevalidateDelayMs = 150;

} // namespace

VehicleFormPresenter::VehicleFormPresenter(IVehicleFormView &form, IVehicleDetailsView &details,
                                           IVehicleConditionsView &conditions,
                                           IVehicleFilesView &files,
                                           const application::VehicleRegistrationService &service,
                                           bool freeNavigation, QObject *parent)
    : QObject(parent)
    , m_form(form)
    , m_details(std::make_unique<VehicleDetailsPresenter>(details, service))
    , m_conditions(std::make_unique<VehicleConditionsPresenter>(conditions, service))
    , m_files(std::make_unique<VehicleFilesPresenter>(files, service))
    , m_nav(kStepCount, freeNavigation)
{
    m_steps = {m_details.get(), m_conditions.get(), m_files.get()};

    m_revalidateTimer.setSingleShot(true);
    m_revalidateTimer.setInterval(kRevalidateDelayMs);
    connect(&m_revalidateTimer, &QTimer::timeout, this,
            &VehicleFormPresenter::revalidateEditedSteps);
}

VehicleFormPresenter::~VehicleFormPresenter() = default;

VehicleDetailsPresenter &VehicleFormPresenter::details()
{
    return *m_details;
}

VehicleFilesPresenter &VehicleFormPresenter::files()
{
    return *m_files;
}

void VehicleFormPresenter::onStepEdited(int step)
{
    if (step < 0 || step >= kStepCount || !editCounts())
        return;
    noteEdit();
    m_editedSteps.insert(step);
    // start() reinicia la cuenta si ya corría: la revalidación espera a que
    // el usuario deje de teclear, en vez de correr con cada cambio.
    m_revalidateTimer.start();
}

void VehicleFormPresenter::noteEdit()
{
}

void VehicleFormPresenter::revalidateEditedSteps()
{
    m_revalidateTimer.stop();
    const QSet<int> editedSteps = std::exchange(m_editedSteps, QSet<int>());
    for (int step : editedSteps) {
        const domain::ValidationResult result = m_steps[step]->validate();
        m_nav.setValid(step, result.isValid());

        // Los errores se muestran solo en un paso ya intentado: antes de eso,
        // nadie quiere ver en rojo un formulario que apenas está llenando.
        if (!m_nav.isAttempted(step))
            continue;

        // Los campos se marcan y se limpian mientras el usuario corrige, pero
        // el foco se queda donde está.
        const domain::ValidationResult unshown = markStepFields(step, result);

        // El aviso habla del paso que se ve.
        if (step == m_nav.current()) {
            if (result.isValid())
                m_form.showMessage(QString());
            else
                showValidationErrors(QStringLiteral("Revisa los campos marcados"), unshown);
        }
    }
    // El stepper sigue a la validez: un paso que deja de ser válido pierde su
    // palomita (o se pinta en rojo), y vuelve cuando se corrige.
    refreshStepper();
}

QString VehicleFormPresenter::invalidStepTitle(int, bool) const
{
    return QStringLiteral("Revisa los campos marcados");
}

void VehicleFormPresenter::showCurrentStep()
{
    m_form.showStep(m_nav.current());
    refreshStepper();
}

void VehicleFormPresenter::applyLookups(const application::RegistrationLookupsDto &lookups)
{
    m_details->setLookups(lookups);
    m_conditions->setLookups(lookups);
}

int VehicleFormPresenter::validateAllSteps(domain::ValidationResult &firstInvalid)
{
    m_nav.markAllAttempted();
    int firstInvalidStep = -1;
    for (int step = 0; step < kStepCount; ++step) {
        domain::ValidationResult result = m_steps[step]->validate();
        m_nav.setValid(step, result.isValid());
        markStepFields(step, result);
        if (!result.isValid() && firstInvalidStep < 0) {
            firstInvalidStep = step;
            firstInvalid = std::move(result);
        }
    }
    return firstInvalidStep;
}

int VehicleFormPresenter::stepOwning(const domain::ValidationResult &result) const
{
    for (int step = 0; step < kStepCount; ++step) {
        for (const domain::ValidationError &error : result.errors()) {
            if (m_steps[step]->ownsField(error.field))
                return step;
        }
    }
    return 0;
}

void VehicleFormPresenter::showInvalidStep(int step, const domain::ValidationResult &result)
{
    const bool alreadyVisible = (step == m_nav.current());
    if (!alreadyVisible)
        m_nav.moveTo(step);
    showCurrentStep();
    // Quien llama ya marcó los campos; marcarlos otra vez no cambia nada y
    // dice qué errores no quedaron a la vista, que son los que lleva el aviso.
    showValidationErrors(invalidStepTitle(step, alreadyVisible), markStepFields(step, result));

    // Solo aquí se mueve el foco, porque el usuario acaba de pedir avanzar o
    // guardar; la revalidación en vivo nunca lo toca, ya que a media captura lo
    // mandaría a teclear en otro campo.
    m_steps[step]->focusFirstError(result);
}

domain::ValidationResult VehicleFormPresenter::markStepFields(int step,
                                                             const domain::ValidationResult &result)
{
    return m_steps[step]->showErrors(result);
}

void VehicleFormPresenter::showValidationErrors(const QString &title,
                                                const domain::ValidationResult &unshown)
{
    // Cada mensaje ya se ve bajo su campo, así que el aviso no los repite:
    // solo pide revisar los campos marcados. Lista los que no tienen campo en
    // pantalla (la UMA, o uno de otro paso), que si no, no se verían en
    // ningún lado. Todos y no solo el primero: quien captura prefiere ver de
    // una vez todo lo que le falta.
    if (unshown.isValid()) {
        m_form.showMessage(title);
        return;
    }
    m_form.showMessage(title + QStringLiteral(":\n• ")
                       + unshown.joinedMessages(QStringLiteral("\n• ")));
}

void VehicleFormPresenter::cancelRevalidation()
{
    m_revalidateTimer.stop();
    m_editedSteps.clear();
}

} // namespace presentation
