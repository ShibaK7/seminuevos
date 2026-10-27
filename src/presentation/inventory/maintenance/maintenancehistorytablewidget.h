#ifndef PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYTABLEWIDGET_H
#define PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYTABLEWIDGET_H

#include "presentation/inventory/maintenance/imaintenancehistoryview.h"

#include <QList>
#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MaintenanceHistoryTableWidget;
}
QT_END_NAMESPACE

namespace presentation {
class MaintenanceHistoryPresenter;
}

class AddMaintenanceDialog;

// Historial de mantenimientos de una unidad: la tarjeta con la tabla (Fecha,
// Tipo de mantenimiento, Costo, Descripción y Recibo), "Agregar Mantenimiento"
// e "Imprimir". Su formulario es maintenancehistorytablewidget.ui (en esta
// misma carpeta), y en el del Paso 1 aparece como sección promovida entre la
// tarjeta del vehículo y la del propietario.
//
// Vista pasiva de MaintenanceHistoryPresenter (IMaintenanceHistoryView): avisa
// los clics de sus botones y lo capturado en el formulario de alta
// (AddMaintenanceDialog), pinta los renglones que el presenter le da y le dice
// cuál está seleccionado para su recibo (US-03.3, tareas #29, #31 y #45 a
// #48).
// Cuándo se ve no lo decide esta clase: el alta nunca la muestra, y el
// expediente del vehículo la muestra según el rol de quien entró.
//
// Solo la Descripción se edita, con doble clic sobre la celda: al terminar
// (Enter o al salir de ella) avisa descriptionEdited y el presenter decide si
// se guarda. Las demás celdas no aceptan edición.
//
// La columna Recibo es una casilla por renglón que dice si ya se imprimió su
// recibo. Solo informa: el usuario no la cambia (sin Qt::ItemIsUserCheckable),
// la marca "Imprimir".
class MaintenanceHistoryTableWidget : public QWidget,
                                      public presentation::IMaintenanceHistoryView
{
    Q_OBJECT

public:
    // El orden de las columnas en el .ui. No es un contrato: solo nombra lo
    // que está en Designer para no repetir números sueltos.
    enum Column {
        DateColumn,
        TypeColumn,
        CostColumn,
        DescriptionColumn,
        ReceiptColumn,
    };

    explicit MaintenanceHistoryTableWidget(QWidget *parent = nullptr);
    ~MaintenanceHistoryTableWidget() override;

    // Sus controles en orden de tabulación, para que el Paso 1 los encadene
    // con los suyos (el orden cruza dos formularios).
    QList<QWidget *> focusOrder() const;

    // Conecta sus botones con el presenter. Solo el expediente lo arma: en el
    // alta la sección no se ve y no tiene presenter.
    void bind(presentation::MaintenanceHistoryPresenter &presenter);

    // --- IMaintenanceHistoryView ---
    void setBusy(bool busy) override;
    void showMaintenances(const QList<application::MaintenanceItemDto> &items) override;
    void checkReceipt(int maintenanceId) override;
    void showWarning(const QString &title, const QString &message) override;
    void showInformation(const QString &message) override;
    bool confirmDescriptionUpdate() override;
    int selectedMaintenanceId() override;
    QString askReceiptPath(const QString &suggestedName) override;
    void openDocument(const QString &path) override;
    void askNewMaintenance(const QList<application::CatalogOptionDto> &maintenanceTypes) override;
    void setMaintenanceFormBusy(bool busy) override;
    void showMaintenanceFormErrors(const QList<domain::ValidationError> &errors) override;
    void closeMaintenanceForm() override;

signals:
    // "+ Agregar Mantenimiento".
    void addMaintenanceRequested();
    // "Imprimir": el recibo del renglón seleccionado.
    void printReceiptsRequested();
    // Aceptar en el formulario de alta, con lo capturado.
    void maintenanceSubmitted(const application::NewMaintenanceDto &maintenance);
    // Se terminó de editar la descripción de un renglón y su texto cambió.
    void descriptionEdited(int maintenanceId, const QString &description);

private:
    Ui::MaintenanceHistoryTableWidget *ui;
    // El formulario de alta abierto, si hay uno. Se borra solo al cerrarse.
    QPointer<AddMaintenanceDialog> m_form;
    // Mientras la vista pinta, los cambios de las celdas no son del usuario.
    bool m_painting = false;
};

#endif // PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYTABLEWIDGET_H
