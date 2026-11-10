#ifndef PRESENTATION_INVENTORY_RECORD_VEHICLERECORDVIEW_H
#define PRESENTATION_INVENTORY_RECORD_VEHICLERECORDVIEW_H

#include "presentation/inventory/record/ivehiclerecordview.h"

#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class VehicleRecordView;
}
QT_END_NAMESPACE

namespace presentation {
class VehicleRecordPresenter;
}

class VehicleConditionsView;
class VehicleDetailsView;
class VehicleFilesView;

// El expediente de un vehículo (US-03.3 y US-03.4): la pantalla que abre
// "Detalles" en una tarjeta del inventario. MainWindow la apila sobre el
// inventario en el mismo mainContentStack, como al asistente de registro.
//
// Es hermana del marco del asistente: su .ui (vehiclerecordview.ui, en esta
// misma carpeta) promueve las MISMAS tres páginas del alta, cada una en su
// área con scroll, pero en vez del stepper con bloqueos lleva pestañas libres
// y en vez de Siguiente/Guardar, Volver y Editar.
//
// Vista pasiva: VehicleRecordPresenter decide todo; esta clase pinta lo que él
// dice (IVehicleRecordView) y le avisa lo que hace el usuario.
class VehicleRecordView : public QWidget, public presentation::IVehicleRecordView
{
    Q_OBJECT

public:
    explicit VehicleRecordView(QWidget *parent = nullptr);
    ~VehicleRecordView() override;

    VehicleDetailsView &detailsView();
    VehicleConditionsView &conditionsView();
    VehicleFilesView &filesView();

    // Conecta pestañas y botones con el presenter, y se queda con él para
    // borrarlo antes que a las páginas (ver el destructor).
    void bind(presentation::VehicleRecordPresenter &presenter);

    // El folio que muestra, o -1 si todavía no tiene presenter.
    int folio() const;
    // Vuelve a leer el expediente (la cáscara, al reabrir el mismo folio).
    void refresh();
    // Si se puede cerrar o reemplazar. Sin presenter no hay nada que perder.
    bool confirmLeave();

    // --- IVehicleRecordView ---
    void showTitle(const QString &title) override;
    void showStep(int step) override;
    void showStepStates(const QList<presentation::StepVisual> &states) override;
    void showMessage(const QString &message) override;
    void showNotice(const QString &notice) override;
    void setBusy(bool busy) override;
    void showMode(presentation::FormMode mode, bool canEdit, bool editEnabled) override;
    bool confirmDiscardChanges() override;
    bool confirmInvoiceRemoval(const QString &fileName) override;
    void setDealTermsVisible(bool visible) override;
    void setMaintenanceHistoryVisible(bool visible) override;

signals:
    // Pide cerrar el expediente y volver al inventario.
    void returnToInventory();
    // La unidad cambió (o ya no existe): el inventario debe releerla.
    void vehicleChanged(int folio);

private:
    Ui::VehicleRecordView *ui;
    // El presenter que llegó a bind(). QPointer: si alguien lo borra antes que
    // a la vista, queda nulo y el destructor no lo vuelve a borrar.
    QPointer<presentation::VehicleRecordPresenter> m_presenter;
};

#endif // PRESENTATION_INVENTORY_RECORD_VEHICLERECORDVIEW_H
