#ifndef PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYTABLEWIDGET_H
#define PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYTABLEWIDGET_H

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MaintenanceHistoryTableWidget;
}
QT_END_NAMESPACE

// Historial de mantenimientos de una unidad: la tarjeta con la tabla (Fecha,
// Tipo de mantenimiento, Costo, Descripción y Recibo), "Agregar Mantenimiento"
// e "Imprimir". Su formulario es maintenancehistorytablewidget.ui (en esta
// misma carpeta), y en el del Paso 1 aparece como sección promovida entre la
// tarjeta del vehículo y la del propietario.
//
// Por ahora es solo la vista: avisa los clics de sus botones y nada más. Qué
// renglones pinta, el alta en línea y el recibo los programa quien la conecta
// (US-03.3, tareas #29, #31 y #45 a #48), implementando aquí su interfaz.
// Cuándo se ve no lo decide esta clase: el alta nunca la muestra, y el
// expediente del vehículo la muestra según el rol de quien entró.
//
// La columna Recibo es una casilla por renglón (Qt::ItemIsUserCheckable) para
// elegir qué comprobantes imprimir.
class MaintenanceHistoryTableWidget : public QWidget
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

signals:
    // "+ Agregar Mantenimiento".
    void addMaintenanceRequested();
    // "Imprimir": los renglones con la casilla de Recibo marcada.
    void printReceiptsRequested();

private:
    Ui::MaintenanceHistoryTableWidget *ui;
};

#endif // PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYTABLEWIDGET_H
