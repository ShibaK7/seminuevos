#ifndef PRESENTATION_INVENTORY_REGISTRATION_VEHICLEWIZARDVIEW_H
#define PRESENTATION_INVENTORY_REGISTRATION_VEHICLEWIZARDVIEW_H

#include "presentation/inventory/registration/ivehiclewizardview.h"

#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class VehicleWizardView;
}
QT_END_NAMESPACE

namespace presentation {
class VehicleWizardPresenter;
}

class WizardStepper;
class VehicleDetailsView;
class VehicleConditionsView;
class VehicleFilesView;

// Vista embebida (NO modal) con los 3 pasos del asistente de registro de
// vehículo (US-03.2). MainWindow la muestra reemplazando la página de
// Inventario dentro del mismo mainContentStack -- no se abre como QDialog.
//
// El marco (título, pila de páginas, aviso y botones) está en
// vehiclewizardview.ui (en esta misma carpeta), con las tres páginas como widgets promovidos.
// El stepper también está en el .ui, promovido.
//
// Es una vista pasiva: no valida, no navega por su cuenta ni habla con el
// servicio. Todo eso lo decide VehicleWizardPresenter; esta clase solo pinta
// lo que él dice (IVehicleWizardView) y le avisa lo que hace el usuario. La
// raíz de composición arma el presenter con esta vista y sus tres páginas, y
// llama a bind().
//
// Emite vehicleRegistered()/returnToInventory() (los reenvía del presenter)
// para que MainWindow decida cuándo regresar a la lista de inventario.
class VehicleWizardView : public QWidget, public presentation::IVehicleWizardView
{
    Q_OBJECT

public:
    explicit VehicleWizardView(QWidget *parent = nullptr);
    ~VehicleWizardView() override;

    VehicleDetailsView &detailsView();
    VehicleConditionsView &conditionsView();
    VehicleFilesView &filesView();

    // Conecta los botones, el stepper y las páginas con el presenter, y se
    // queda con él para borrarlo antes que a las páginas (ver el destructor).
    void bind(presentation::VehicleWizardPresenter &presenter);

    // --- IVehicleWizardView ---
    void showStep(int step) override;
    void showStepIndicators(const QList<presentation::StepIndicator> &indicators) override;
    void showMessage(const QString &message) override;
    void setPrimaryAction(presentation::PrimaryAction action) override;
    void setBusy(bool busy) override;
    void showRegistered(bool canPrintContract) override;
    presentation::AfterRegistration askAfterRegistration(int folio, bool canPrintContract) override;
    QString askContractPath(const QString &suggestedName) override;
    void openDocument(const QString &path) override;
    void showContractError(const QString &message) override;
    bool confirmDiscard() override;

signals:
    // Pide cerrar esta pantalla y volver al inventario. Se emite al cancelar
    // (con confirmación si todavía no se guardó), al pulsar "Volver al
    // Inventario", y también sola después de un registro exitoso.
    void returnToInventory();
    // Se emite en cuanto la unidad queda guardada, antes de cerrar, para que
    // el inventario de atrás ya esté actualizado cuando se vuelva a ver.
    void vehicleRegistered(int folio);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    Ui::VehicleWizardView *ui;
    // El presenter que llegó a bind(). QPointer y no puntero crudo: si alguien
    // lo borra antes que a la vista, queda nulo y el destructor no lo vuelve a
    // borrar.
    QPointer<presentation::VehicleWizardPresenter> m_presenter;
};

#endif // PRESENTATION_INVENTORY_REGISTRATION_VEHICLEWIZARDVIEW_H
