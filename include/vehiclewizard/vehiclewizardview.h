#ifndef VEHICLEWIZARDVIEW_H
#define VEHICLEWIZARDVIEW_H

#include <QWidget>

#include <memory>

namespace domain {
class Vehicle;
}

class WizardStepper;
class QStackedWidget;
class Step1DetailsView;
class Step2ConditionView;
class Step3FilesView;
class QPushButton;
class QLabel;
class VehicleRegistrationWorker;

// Vista embebida (NO modal) con los 3 pasos del wizard de registro de
// vehículo (US-03.2). MainWindow la muestra reemplazando la página de
// Inventario dentro del mismo mainContentStack -- no se abre como QDialog.
// Al confirmar el Paso 3 arma la unidad con un VehicleBuilder a partir de lo
// que tienen los widgets, dispara el guardado atómico (archivos + BD) y
// ofrece imprimir el contrato. Emite vehicleRegistered()/returnToInventory() para
// que MainWindow decida cuándo regresar a la lista de inventario.
class VehicleWizardView : public QWidget
{
    Q_OBJECT

public:
    explicit VehicleWizardView(QWidget *parent = nullptr);
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
    void onStepClicked(int index);
    void onSaveClicked();
    void onCancelClicked();
    void onRegistrationSucceeded(int folio);
    void onRegistrationFailed(const QString &reason);
    void onPrintContractClicked();

private:
    void goToStep(int index);
    void showError(const QString &message);
    void hideError();
    void setBusy(bool busy);
    // Pide destino y genera el PDF. Devuelve false si el usuario canceló el
    // diálogo de guardado o si falló la generación, para que quien lo llame
    // pueda dejarlo intentar de nuevo en vez de cerrar la pantalla.
    bool printContract();
    bool eventFilter(QObject *watched, QEvent *event) override;

    WizardStepper *m_stepper;
    QStackedWidget *m_stack;
    Step1DetailsView *m_step1;
    Step2ConditionView *m_step2;
    Step3FilesView *m_step3;

    QLabel *m_errorLabel;
    QPushButton *m_cancelButton;
    QPushButton *m_saveButton;
    QPushButton *m_printContractButton;

    int m_currentStep = 0;
    // Paso más lejano ya validado/guardado -- distinto de m_currentStep
    // (qué página se muestra ahora) para que regresar a un paso anterior no
    // "bloquee" el acceso a los pasos que ya se habían completado.
    int m_maxUnlockedStep = 0;
    bool m_registered = false;
    // La unidad ya construida y validada. La vista conserva SU ejemplar: al
    // worker se le manda un clon, porque ese hilo reescribe las rutas de los
    // archivos y compartir el objeto rompería un reintento tras un fallo.
    std::unique_ptr<domain::Vehicle> m_vehicle;
    VehicleRegistrationWorker *m_worker = nullptr;
};

#endif // VEHICLEWIZARDVIEW_H
