#ifndef PRESENTATION_PRESENTERS_IVEHICLEWIZARDVIEW_H
#define PRESENTATION_PRESENTERS_IVEHICLEWIZARDVIEW_H

#include "presentation/navigation/wizardnavigator.h"

#include <QList>
#include <QString>

namespace presentation {

// Qué dice y hace el botón primario del asistente.
enum class PrimaryAction {
    Loading, // leyendo catálogos
    Next,    // pasos 1 y 2
    Save,    // paso 3
    Saving,  // registro en curso
    Saved,   // ya quedó registrado
};

// Qué eligió el usuario en el aviso de "vehículo registrado".
enum class AfterRegistration {
    PrintContract,
    BackToInventory,
};

// Cómo se ve un paso en el stepper.
struct StepIndicator
{
    StepVisual visual = StepVisual::Pending;
    bool complete = false; // palomita
    QString hint;          // tooltip de un paso bloqueado
};

// El marco del asistente: stepper, aviso y botones. Las páginas de cada paso
// tienen su propia interfaz (IVehicleDetailsView y compañía).
class IVehicleWizardView
{
public:
    virtual ~IVehicleWizardView() = default;

    virtual void showStep(int step) = 0;
    virtual void showStepIndicators(const QList<StepIndicator> &indicators) = 0;
    // El aviso bajo las páginas. Vacío = ocultarlo.
    virtual void showMessage(const QString &message) = 0;
    virtual void setPrimaryAction(PrimaryAction action) = 0;
    // Ocupado: stepper, páginas y botones deshabilitados.
    virtual void setBusy(bool busy) = 0;
    // La unidad ya se guardó: las páginas se quedan deshabilitadas, el botón
    // de cancelar pasa a "Volver al Inventario" y el del contrato se habilita
    // si hay contrato.
    virtual void showRegistered(bool canPrintContract) = 0;

    virtual AfterRegistration askAfterRegistration(int folio, bool canPrintContract) = 0;
    // Dónde guardar el contrato. Vacío si canceló.
    virtual QString askContractPath(const QString &suggestedName) = 0;
    virtual void openDocument(const QString &path) = 0;
    virtual void showContractError(const QString &message) = 0;
    // ¿Descartar lo capturado?
    virtual bool confirmDiscard() = 0;

protected:
    IVehicleWizardView() = default;
    IVehicleWizardView(const IVehicleWizardView &) = default;
    IVehicleWizardView &operator=(const IVehicleWizardView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_IVEHICLEWIZARDVIEW_H
