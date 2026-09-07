#ifndef VEHICLEWIZARDVIEW_H
#define VEHICLEWIZARDVIEW_H

#include "vehicledraft.h"

#include <QWidget>

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
// Mantiene un VehicleDraft en memoria hasta que el Paso 3 confirma -- ahí
// dispara el guardado atómico (archivos + BD) y habilita "Imprimir
// Contrato". Emite cancelled()/vehicleRegistered() para que MainWindow
// decida cuándo regresar a la lista de inventario.
class VehicleWizardView : public QWidget
{
    Q_OBJECT

public:
    explicit VehicleWizardView(QWidget *parent = nullptr);

signals:
    // Se emite al presionar "Cancelar" (con confirmación si aún no se ha
    // guardado) o "Volver al Inventario" después de un guardado exitoso.
    void cancelled();
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
    VehicleDraft m_finalDraft;
    VehicleRegistrationWorker *m_worker = nullptr;
};

#endif // VEHICLEWIZARDVIEW_H
