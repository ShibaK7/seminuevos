#ifndef PRESENTATION_VIEWS_INVENTORY_INVENTORYVIEW_H
#define PRESENTATION_VIEWS_INVENTORY_INVENTORYVIEW_H

#include "presentation/presenters/iinventoryview.h"

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class InventoryView;
}
QT_END_NAMESPACE

namespace presentation {
class InventoryPresenter;
}

class QFrame;

// La página del inventario: pestañas, barra de filtros y la rejilla de
// tarjetas. Su formulario es src/ui/inventoryview.ui, y en mainwindow.ui
// aparece como un widget promovido dentro de la pila de contenido.
//
// Es una vista pasiva (IInventoryView): no lee la base ni el disco.
// InventoryPresenter le pide los filtros, lee fuera del hilo de la interfaz y
// le dice qué tarjetas pintar.
class InventoryView : public QWidget, public presentation::IInventoryView
{
    Q_OBJECT

public:
    explicit InventoryView(QWidget *parent = nullptr);
    ~InventoryView() override;

    // Conecta los filtros con el presenter.
    void bind(presentation::InventoryPresenter &presenter);

    // --- IInventoryView ---
    application::InventoryFilterDto filter() const override;
    void showVehicles(const QList<application::InventoryItemDto> &vehicles) override;
    void showInventoryMessage(const QString &message) override;
    void scrollToFolio(int folio) override;

    // Para que la ventana arme el orden de tabulación completo, que cruza dos
    // formularios: los filtros y la lista van antes que el menú lateral, y las
    // pestañas y "Agregar Vehículo" después.
    QList<QWidget *> leadingFocusWidgets() const;
    QList<QWidget *> trailingFocusWidgets() const;

signals:
    // El botón "Agregar Vehículo". Abrir el asistente le toca a la ventana.
    void addVehicleRequested();

private:
    // La línea gris bajo las pestañas es lo único de esa fila que no viene del
    // .ui: el subrayado de la pestaña activa tiene que quedar encima de ella,
    // y eso no se puede expresar con un layout.
    void createNavDivider();
    // Recoloca el divisor. Va por filtro de eventos porque no está en un
    // layout: se superpone a la fila de pestañas.
    bool eventFilter(QObject *watched, QEvent *event) override;
    void layoutNavDivider();

    void showEvent(QShowEvent *event) override;

    void populateStatusFilter();
    void applyDefaultDateRange();

    Ui::InventoryView *ui;
    QFrame *m_navDivider = nullptr;
    // El tamaño del filtro de estado se fija una sola vez, al primer show.
    bool m_filterSized = false;
};

#endif // PRESENTATION_VIEWS_INVENTORY_INVENTORYVIEW_H
