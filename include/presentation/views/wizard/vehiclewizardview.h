#ifndef PRESENTATION_VIEWS_WIZARD_VEHICLEWIZARDVIEW_H
#define PRESENTATION_VIEWS_WIZARD_VEHICLEWIZARDVIEW_H

#include "application/dto/catalogdtos.h"
#include "application/dto/registrationdtos.h"
#include "presentation/navigation/wizardnavigator.h"

#include <QPointer>
#include <QSet>
#include <QWidget>

#include <optional>

namespace application {
class VehicleRegistrationService;
}
namespace presentation {
class TaskRunner;
}

class WizardStepper;
class QStackedWidget;
class VehicleDetailsView;
class VehicleConditionsView;
class VehicleFilesView;
class QPushButton;
class QLabel;
class QTimer;

// Vista embebida (NO modal) con los 3 pasos del wizard de registro de
// vehículo (US-03.2). MainWindow la muestra reemplazando la página de
// Inventario dentro del mismo mainContentStack -- no se abre como QDialog.
// Al confirmar el Paso 3 arma un VehicleRegistrationDto con lo que tienen los
// pasos y lo registra por el servicio de aplicación (fuera del hilo de la
// interfaz), y ofrece imprimir el contrato que devuelve.
//
// Hasta que exista su presenter, la vista habla directo con el servicio; por
// eso recibe el servicio y el TaskRunner por constructor (los inyecta la raíz
// de composición a través de MainWindow). Emite vehicleRegistered()/returnToInventory() para
// que MainWindow decida cuándo regresar a la lista de inventario.
//
// Navegación entre pasos (el estado lo lleva m_nav, un WizardNavigator):
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
// Qué hace válido a un paso no se decide aquí: cada paso arma un
// VehicleBuilder desechable con lo capturado y le pregunta al dominio.
class VehicleWizardView : public QWidget
{
    Q_OBJECT

public:
    VehicleWizardView(application::VehicleRegistrationService &service,
                      presentation::TaskRunner &runner, bool freeNavigation,
                      QWidget *parent = nullptr);
    ~VehicleWizardView() override;

signals:
    // Pide cerrar esta pantalla y volver al inventario. Se emite al cancelar
    // (con confirmación si todavía no se guardó), al pulsar "Volver al
    // Inventario", y también sola después de un registro exitoso.
    void returnToInventory();
    // Se emite en cuanto la unidad queda guardada, antes de cerrar, para que
    // el inventario de atrás ya esté actualizado cuando se vuelva a ver.
    void vehicleRegistered(int folio);

private slots:
    // Clic en un paso del stepper: hacia atrás va directo; hacia adelante,
    // incluso a un paso bloqueado, pasa por advanceTo(), que explica qué falta.
    void onStepClicked(int index);
    // El botón primario: "Siguiente" en los dos primeros pasos y "Guardar" en
    // el último.
    void onPrimaryClicked();
    void onCancelClicked();
    void onRegistrationSucceeded(int folio);
    void onRegistrationFailed(const QString &reason);
    void onPrintContractClicked();

private:
    // Intenta llegar a `target`, posterior al paso actual. Valida y marca como
    // intentado cada paso desde el actual hasta el anterior a `target`; en el
    // primero inválido se queda ahí y lo explica. Si todos son válidos, entra.
    void advanceTo(int target);
    // Pide los catálogos al servicio fuera del hilo de la interfaz y, al
    // llegar, llena los pasos y hace la validación inicial.
    void loadLookups();
    void onLookupsLoaded(const application::RegistrationLookupsDto &lookups);
    void onRegistrationFinished(const application::RegistrationResult &result);
    // El paso al que pertenecen los errores de un rechazo al guardar.
    int stepOwningErrors(const domain::ValidationResult &result) const;
    // Algo cambió en el paso `step`. No revalida en el acto: arranca (o
    // reinicia) un debounce, y revalidateEditedSteps() hace el trabajo cuando
    // el usuario deja de teclear.
    void onStepEdited(int step);
    void revalidateEditedSteps();
    // Le pregunta al servicio (y este al dominio) por lo que el paso `step`
    // tiene capturado ahora. Es pura: no hace E/S.
    domain::ValidationResult validateStep(int step) const;
    // Muestra el paso actual del navegador: la página, el stepper y el texto
    // del botón primario.
    void showCurrentStep();
    // Lleva al usuario al paso `step`, que no pasó la validación, lo explica
    // en el aviso con todos sus mensajes y le da el foco al primer campo que
    // falla. Solo para intentos explícitos (avanzar o guardar), y con los
    // campos ya marcados por markStepFields().
    void showInvalidStep(int step, const domain::ValidationResult &result);
    // Marca en la página del paso `step` los campos que fallaron y limpia los
    // demás. Solo se llama con pasos ya intentados: antes de eso no se pinta
    // nada en rojo. No mueve el foco.
    void markStepFields(int step, const domain::ValidationResult &result);
    void refreshStepper();
    void refreshPrimaryButton();
    // El aviso con un título y, debajo, todos los mensajes del resultado.
    void showValidationErrors(const QString &title, const domain::ValidationResult &result);
    void showError(const QString &message);
    void hideError();
    void setBusy(bool busy);
    // Pide destino y genera el PDF. Devuelve false si el usuario canceló el
    // diálogo de guardado o si falló la generación, para que quien lo llame
    // pueda dejarlo intentar de nuevo en vez de cerrar la pantalla.
    bool printContract();
    bool eventFilter(QObject *watched, QEvent *event) override;

    // Qué paso se muestra, cuáles son válidos y cuáles ya se intentaron.
    // Sustituye al "paso más lejano desbloqueado" que había antes, que solo
    // crecía: un paso ya validado seguía abierto aunque después se borrara lo
    // que lo hacía válido.
    application::VehicleRegistrationService &m_service;
    presentation::TaskRunner &m_runner;
    presentation::WizardNavigator m_nav;

    WizardStepper *m_stepper;
    QStackedWidget *m_stack;
    VehicleDetailsView *m_step1;
    VehicleConditionsView *m_step2;
    VehicleFilesView *m_step3;

    QLabel *m_errorLabel;
    QPushButton *m_cancelButton;
    QPushButton *m_primaryButton;
    QPushButton *m_printContractButton;

    // Debounce de la revalidación en vivo: un solo temporizador y los pasos
    // que se editaron mientras corría.
    QTimer *m_revalidateTimer;
    QSet<int> m_editedSteps;

    bool m_registered = false;
    // Hay un registro corriendo en el TaskRunner.
    bool m_saving = false;
    // UMA vigente, leída con los catálogos. Sin ella no se valida el tope de
    // efectivo y el pago en efectivo se rechaza.
    std::optional<double> m_umaDailyValue;
    // Lo que el contrato dice, ya decidido por el dominio al registrar. Es un
    // valor: la vista no se queda con la entidad.
    std::optional<domain::ContractData> m_contract;
};

#endif // PRESENTATION_VIEWS_WIZARD_VEHICLEWIZARDVIEW_H
