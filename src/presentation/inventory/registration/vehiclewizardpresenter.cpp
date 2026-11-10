#include "presentation/inventory/registration/vehiclewizardpresenter.h"

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/inventory/registration/ivehiclewizardview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionspresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"
#include "presentation/common/tasks/taskrunner.h"

namespace presentation {

VehicleWizardPresenter::VehicleWizardPresenter(IVehicleWizardView &wizard,
                                               IVehicleDetailsView &details,
                                               IVehicleConditionsView &conditions,
                                               IVehicleFilesView &files,
                                               const application::VehicleRegistrationService &service,
                                               TaskRunner &runner, bool freeNavigation,
                                               QObject *parent)
    : VehicleFormPresenter(wizard, details, conditions, files, service, freeNavigation, parent)
    , m_wizard(wizard)
    , m_service(service)
    , m_runner(runner)
{
}

VehicleWizardPresenter::~VehicleWizardPresenter() = default;

void VehicleWizardPresenter::start()
{
    m_details->start();
    m_files->start();
    showCurrentStep();

    // Los catálogos se leen fuera del hilo de la interfaz: mientras llegan, el
    // asistente está ocupado y no se puede capturar.
    m_wizard.setBusy(true);
    m_wizard.setPrimaryAction(PrimaryAction::Loading);
    const application::VehicleRegistrationService *service = &m_service;
    m_runner.run(this, [service] { return service->loadLookups(); },
                 [this](const application::RegistrationLookupsDto &lookups) {
                     onLookupsLoaded(lookups);
                 });
}

void VehicleWizardPresenter::onLookupsLoaded(const application::RegistrationLookupsDto &lookups)
{
    applyLookups(lookups);
    m_loaded = true;
    setBusy(false);
    if (!lookups.errorMessage.isEmpty()) {
        m_wizard.showMessage(QStringLiteral("No se pudieron leer todos los catálogos:\n")
                             + lookups.errorMessage);
    } else if (!lookups.umaDailyValue) {
        m_wizard.showMessage(QStringLiteral(
            "No hay UMA configurada: no se podrá registrar una compra pagada en efectivo."));
    }

    // Validación inicial en silencio: sin marcar ningún paso como intentado,
    // no se pinta nada en rojo ni se mueve el foco. Solo sirve para que el
    // stepper sepa desde el principio qué pasos están bloqueados. Lo que se
    // haya avisado como editado mientras se llenaban los combos ya quedó
    // cubierto aquí.
    cancelRevalidation();
    for (int step = 0; step < kStepCount; ++step)
        m_nav.setValid(step, m_steps[step]->validate().isValid());
    refreshStepper();
}

void VehicleWizardPresenter::onStepClicked(int step)
{
    const int current = m_nav.current();
    if (step == current)
        return;

    if (step < current) {
        // Regresar siempre se puede y no valida nada: corregir un paso
        // anterior no tiene por qué exigir que el actual ya esté completo.
        m_nav.moveTo(step);
        showCurrentStep();
        m_wizard.showMessage(QString());
        return;
    }

    // Hacia adelante, aunque el paso se vea bloqueado: advanceTo() valida lo
    // que hay en medio y, si algo falta, dice qué.
    advanceTo(step);
}

void VehicleWizardPresenter::onPrimaryAction()
{
    if (m_saving || m_registered)
        return;

    if (m_nav.current() < kStepCount - 1) {
        advanceTo(m_nav.current() + 1);
        return;
    }

    // Guardar: se revisan los tres pasos, no solo el actual, y los tres
    // quedan como intentados. Así cualquier paso con datos pendientes se
    // marca en el stepper, aunque la navegación libre haya dejado pasar sin
    // validar.
    domain::ValidationResult firstInvalidResult;
    const int firstInvalidStep = validateAllSteps(firstInvalidResult);
    if (firstInvalidStep >= 0) {
        showInvalidStep(firstInvalidStep, firstInvalidResult);
        return;
    }
    refreshStepper();

    // Se vuelven a leer los tres pasos: la fuente de verdad son las vistas,
    // así que volver atrás y corregir algo se refleja sin mantener nada
    // sincronizado. El registro (validar, copiar archivos, guardar) corre en el
    // servicio, fuera del hilo de la interfaz.
    const application::VehicleRegistrationDto registration{m_details->dto(), m_conditions->dto(),
                                                           m_files->dto()};
    m_saving = true;
    setBusy(true);
    m_wizard.showMessage(QString());
    const application::VehicleRegistrationService *service = &m_service;
    m_runner.run(this, [service, registration] { return service->registerVehicle(registration); },
                 [this](const application::RegistrationResult &result) {
                     m_saving = false;
                     onRegistrationFinished(result);
                 });
}

void VehicleWizardPresenter::onRegistrationFinished(const application::RegistrationResult &result)
{
    switch (result.status) {
    case application::RegistrationResult::Status::Registered:
        m_contract = result.contract;
        onRegistrationSucceeded(result.folio);
        return;
    case application::RegistrationResult::Status::Rejected: {
        // Las reglas se revisaron otra vez al guardar (con la UMA vigente y
        // contra la base, como un VIN repetido). Los errores se llevan al paso
        // que tiene el campo.
        setBusy(false);
        const int step = stepOwning(result.validation);
        m_nav.setValid(step, false);
        markStepFields(step, result.validation);
        showInvalidStep(step, result.validation);
        return;
    }
    case application::RegistrationResult::Status::Failed:
        // Disco o base: se queda donde está, y "Guardar" reintenta.
        setBusy(false);
        m_wizard.showMessage(result.errorMessage);
        return;
    }
}

void VehicleWizardPresenter::onRegistrationSucceeded(int folio)
{
    m_registered = true;
    // La vista se queda deshabilitada: la unidad ya quedó guardada, y
    // cualquier cambio ya no llegaría a la base. Solo vuelven el botón para
    // salir y, si hay contrato, el de imprimirlo. Los tres pasos con palomita:
    // Guardar los marcó como intentados y los tres pasaron la validación.
    const bool canPrint = m_contract.has_value();
    m_wizard.setPrimaryAction(PrimaryAction::Saved);
    m_wizard.showRegistered(canPrint);
    refreshStepper();

    // El contrato se ofrece AQUÍ, que es cuando los datos de la operación
    // están completos y a la mano. Después de esto se vuelve al inventario, y
    // reconstruirlos desde la base para reimprimir sería otro trabajo.
    const AfterRegistration choice = m_wizard.askAfterRegistration(folio, canPrint);

    emit vehicleRegistered(folio);

    if (choice == AfterRegistration::PrintContract && !printContract()) {
        // No se pudo generar (o se canceló el diálogo de guardado): se deja la
        // pantalla abierta con el botón de contrato disponible, en vez de
        // volver al inventario y perder la oportunidad.
        return;
    }
    emit closeRequested();
}

void VehicleWizardPresenter::onPrintContract()
{
    printContract();
}

bool VehicleWizardPresenter::printContract()
{
    if (!m_contract)
        return false;

    const QString outputPath =
        m_wizard.askContractPath(QStringLiteral("contrato_%1.pdf").arg(m_contract->fileNameHint));
    if (outputPath.isEmpty())
        return false;

    const auto outcome = m_service.generateContract(*m_contract, outputPath);
    if (!outcome.ok) {
        m_wizard.showContractError(outcome.errorMessage);
        return false;
    }
    m_wizard.openDocument(outputPath);
    return true;
}

void VehicleWizardPresenter::onCancel()
{
    if (m_saving)
        return; // el botón ya está deshabilitado, por si acaso

    if (!m_registered && !m_wizard.confirmDiscard())
        return;
    emit closeRequested();
}

void VehicleWizardPresenter::advanceTo(int target)
{
    if (target <= m_nav.current() || target >= m_nav.stepCount())
        return;

    // Navegación libre (WIZARD_FREE_NAVIGATION, solo desarrollo): se entra sin
    // validar, que es justo lo que promete la bandera. Guardar sigue
    // validando los tres pasos.
    if (m_nav.freeNavigation()) {
        m_nav.goTo(target);
        showCurrentStep();
        m_wizard.showMessage(QString());
        return;
    }

    for (int step = m_nav.current(); step < target; ++step) {
        const domain::ValidationResult result = m_steps[step]->validate();
        m_nav.setValid(step, result.isValid());
        m_nav.markAttempted(step);
        // Ya intentado, el paso marca sus campos. Si es válido, esto limpia lo
        // que quedara de un intento anterior sin esperar a la revalidación en
        // vivo, que corre después del debounce.
        markStepFields(step, result);
        if (!result.isValid()) {
            // Los pasos posteriores ni se validan: el usuario se queda en el
            // primero que falta y ve todo lo que le falta a ese.
            showInvalidStep(step, result);
            return;
        }
    }

    // Todo lo que hay entre el paso actual y el destino es válido, y lo
    // anterior al actual también: no se llega a un paso sin haber pasado por
    // los de antes.
    m_nav.goTo(target);
    showCurrentStep();
    m_wizard.showMessage(QString());
}

bool VehicleWizardPresenter::editCounts() const
{
    return m_loaded && !m_registered;
}

QString VehicleWizardPresenter::invalidStepTitle(int step, bool alreadyVisible) const
{
    // Si el paso inválido es el que ya se ve, el aviso solo pide revisarlo. Si
    // no, el asistente llevó al usuario hasta él, y el aviso dice por qué está
    // ahí y no en el paso que pidió.
    return alreadyVisible ? QStringLiteral("Revisa los campos marcados") : blockedHint(step);
}

void VehicleWizardPresenter::showCurrentStep()
{
    VehicleFormPresenter::showCurrentStep();
    refreshPrimaryAction();
}

void VehicleWizardPresenter::refreshStepper()
{
    QList<StepIndicator> indicators;
    for (int step = 0; step < kStepCount; ++step) {
        StepIndicator indicator;
        indicator.visual = m_nav.visual(step);
        indicator.complete = m_nav.isComplete(step);
        // Solo un paso bloqueado lleva tooltip, y nombra el primer paso
        // anterior que falta: es el que hay que completar antes que nada.
        if (indicator.visual == StepVisual::Locked)
            indicator.hint = blockedHint(m_nav.firstBlockingStep(step));
        indicators << indicator;
    }
    m_wizard.showStepIndicators(indicators);
}

void VehicleWizardPresenter::refreshPrimaryAction()
{
    if (m_registered)
        m_wizard.setPrimaryAction(PrimaryAction::Saved);
    else if (m_saving)
        m_wizard.setPrimaryAction(PrimaryAction::Saving);
    else if (m_nav.current() == kStepCount - 1)
        m_wizard.setPrimaryAction(PrimaryAction::Save);
    else
        m_wizard.setPrimaryAction(PrimaryAction::Next);
}

void VehicleWizardPresenter::setBusy(bool busy)
{
    m_wizard.setBusy(busy);
    refreshPrimaryAction();
}

QString VehicleWizardPresenter::blockedHint(int blockingStep) const
{
    if (blockingStep < 0 || blockingStep >= kStepCount)
        return QString();
    return QStringLiteral("Completa «%1» para continuar").arg(m_steps[blockingStep]->title());
}

} // namespace presentation
