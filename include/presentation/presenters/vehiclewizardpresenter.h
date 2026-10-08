#ifndef PRESENTATION_PRESENTERS_VEHICLEWIZARDPRESENTER_H
#define PRESENTATION_PRESENTERS_VEHICLEWIZARDPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "domain/inventory/value_objects/contractdata.h"
#include "presentation/navigation/wizardnavigator.h"

#include <QObject>
#include <QSet>
#include <QTimer>

#include <array>
#include <memory>
#include <optional>

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleConditionsView;
class IVehicleDetailsView;
class IVehicleFilesView;
class IVehicleWizardView;
class TaskRunner;
class VehicleConditionsPresenter;
class VehicleDetailsPresenter;
class VehicleFilesPresenter;
class WizardStepPresenter;

// Presenter del asistente de registro (US-03.2). Lleva la navegación entre
// pasos, la validación en vivo y el registro; las vistas solo muestran lo que
// este presenter les dice y le avisan lo que hace el usuario. Sin widgets
// (solo QtCore), así que se prueba con vistas falsas.
//
// Navegación (el estado lo lleva un WizardNavigator):
//   - Regresar a un paso anterior siempre se puede, sin validar nada.
//   - Avanzar ("Siguiente" o un clic hacia adelante en el stepper) valida
//     desde el paso actual hasta el anterior al destino y se detiene en el
//     primero inválido: marca sus campos, le da el foco al primero y pone
//     todos sus mensajes en el aviso.
//   - La palomita significa "intentado y válido ahora": cada edición
//     revalida su paso, así que se apaga y se vuelve a encender sola. En un
//     paso ya intentado, los campos marcados también siguen a la edición,
//     pero el foco no se mueve.
//   - Al abrir, los pasos se validan en silencio: nada en rojo, solo los
//     bloqueos del stepper.
// Qué hace válido a un paso no se decide aquí: lo dice el dominio, a través
// del servicio.
class VehicleWizardPresenter final : public QObject
{
    Q_OBJECT

public:
    VehicleWizardPresenter(IVehicleWizardView &wizard, IVehicleDetailsView &details,
                           IVehicleConditionsView &conditions, IVehicleFilesView &files,
                           const application::VehicleRegistrationService &service,
                           TaskRunner &runner, bool freeNavigation, QObject *parent = nullptr);
    ~VehicleWizardPresenter() override;

    VehicleDetailsPresenter &details();
    VehicleFilesPresenter &files();

    // Pide los catálogos (fuera del hilo de la interfaz) y, al llegar, llena
    // los pasos y hace la validación inicial en silencio.
    void start();

    // Clic en un paso del stepper: hacia atrás va directo; hacia adelante,
    // incluso a un paso bloqueado, valida lo que hay en medio y explica qué
    // falta.
    void onStepClicked(int step);
    // El botón primario: "Siguiente" en los dos primeros pasos y "Guardar" en
    // el último.
    void onPrimaryAction();
    // Algo cambió en el paso `step`. No revalida en el acto: arranca (o
    // reinicia) un debounce, y la revalidación corre cuando el usuario deja de
    // teclear.
    void onStepEdited(int step);
    void onCancel();
    void onPrintContract();

    // Corre ya la revalidación pendiente, sin esperar el debounce.
    void revalidateEditedSteps();

signals:
    // Se emite en cuanto la unidad queda guardada, antes de cerrar, para que
    // el inventario de atrás ya esté actualizado cuando se vuelva a ver.
    void vehicleRegistered(int folio);
    // Pide cerrar el asistente y volver al inventario.
    void closeRequested();

private:
    void onLookupsLoaded(const application::RegistrationLookupsDto &lookups);
    void onRegistrationFinished(const application::RegistrationResult &result);
    void onRegistrationSucceeded(int folio);
    void advanceTo(int target);
    // Lleva al usuario al paso `step`, que no pasó la validación, lo explica
    // en el aviso con todos sus mensajes y le da el foco al primer campo que
    // falla. Solo para intentos explícitos (avanzar o guardar).
    void showInvalidStep(int step, const domain::ValidationResult &result);
    // Marca en el paso `step` los campos que fallaron y limpia los demás. Solo
    // con pasos ya intentados: antes de eso no se pinta nada en rojo.
    void markStepFields(int step, const domain::ValidationResult &result);
    void showValidationErrors(const QString &title, const domain::ValidationResult &result);
    void showCurrentStep();
    void refreshStepper();
    void refreshPrimaryAction();
    void setBusy(bool busy);
    int stepOwning(const domain::ValidationResult &result) const;
    QString blockedHint(int blockingStep) const;
    // Pide destino y genera el PDF. Devuelve false si el usuario canceló o si
    // falló, para que quien lo llama lo deje intentar de nuevo.
    bool printContract();

    IVehicleWizardView &m_wizard;
    const application::VehicleRegistrationService &m_service;
    TaskRunner &m_runner;

    std::unique_ptr<VehicleDetailsPresenter> m_details;
    std::unique_ptr<VehicleConditionsPresenter> m_conditions;
    std::unique_ptr<VehicleFilesPresenter> m_files;
    // Los tres en el orden en que se muestran.
    std::array<WizardStepPresenter *, 3> m_steps{};

    WizardNavigator m_nav;

    // Debounce de la revalidación en vivo: un solo temporizador y los pasos
    // que se editaron mientras corría.
    QTimer m_revalidateTimer;
    QSet<int> m_editedSteps;

    bool m_loaded = false;
    bool m_saving = false;
    bool m_registered = false;
    // Lo que el contrato dice, ya decidido por el dominio al registrar.
    std::optional<domain::ContractData> m_contract;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_VEHICLEWIZARDPRESENTER_H
