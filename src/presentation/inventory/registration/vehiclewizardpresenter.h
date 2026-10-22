#ifndef PRESENTATION_INVENTORY_REGISTRATION_VEHICLEWIZARDPRESENTER_H
#define PRESENTATION_INVENTORY_REGISTRATION_VEHICLEWIZARDPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "domain/inventory/value_objects/contractdata.h"
#include "presentation/inventory/registration/vehicleformpresenter.h"

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

// Presenter del asistente de registro (US-03.2). Lleva la navegación entre
// pasos, la validación en vivo y el registro; las vistas solo muestran lo que
// este presenter les dice y le avisan lo que hace el usuario. Sin widgets
// (solo QtCore), así que se prueba con vistas falsas. Los pasos, el ruteo de
// errores y la revalidación los comparte con el expediente (VehicleFormPresenter).
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
class VehicleWizardPresenter final : public VehicleFormPresenter
{
    Q_OBJECT

public:
    VehicleWizardPresenter(IVehicleWizardView &wizard, IVehicleDetailsView &details,
                           IVehicleConditionsView &conditions, IVehicleFilesView &files,
                           const application::VehicleRegistrationService &service,
                           TaskRunner &runner, bool freeNavigation, QObject *parent = nullptr);
    ~VehicleWizardPresenter() override;

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
    void onCancel();
    void onPrintContract();

signals:
    // Se emite en cuanto la unidad queda guardada, antes de cerrar, para que
    // el inventario de atrás ya esté actualizado cuando se vuelva a ver.
    void vehicleRegistered(int folio);
    // Pide cerrar el asistente y volver al inventario.
    void closeRequested();

private:
    // --- VehicleFormPresenter ---
    // Mientras se llenan los catálogos, los combos también avisan cambios (la
    // validación inicial ya los cubre), y ya registrada la unidad nada cambia.
    bool editCounts() const override;
    void refreshStepper() override;
    // Si el paso inválido no es el que se ve, el aviso dice por qué se llevó
    // al usuario ahí y no al paso que pidió.
    QString invalidStepTitle(int step, bool alreadyVisible) const override;
    void showCurrentStep() override;

    void onLookupsLoaded(const application::RegistrationLookupsDto &lookups);
    void onRegistrationFinished(const application::RegistrationResult &result);
    void onRegistrationSucceeded(int folio);
    void advanceTo(int target);
    void refreshPrimaryAction();
    void setBusy(bool busy);
    QString blockedHint(int blockingStep) const;
    // Pide destino y genera el PDF. Devuelve false si el usuario canceló o si
    // falló, para que quien lo llama lo deje intentar de nuevo.
    bool printContract();

    IVehicleWizardView &m_wizard;
    const application::VehicleRegistrationService &m_service;
    TaskRunner &m_runner;

    bool m_loaded = false;
    bool m_saving = false;
    bool m_registered = false;
    // Lo que el contrato dice, ya decidido por el dominio al registrar.
    std::optional<domain::ContractData> m_contract;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_VEHICLEWIZARDPRESENTER_H
