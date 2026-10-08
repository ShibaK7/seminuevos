#ifndef PRESENTATION_VIEWS_SHELL_MAINWINDOW_H
#define PRESENTATION_VIEWS_SHELL_MAINWINDOW_H

#include "application/dto/authdtos.h"
#include "presentation/presenters/iinventoryview.h"

#include <QMainWindow>
#include <QPointer>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

namespace presentation {
class InventoryPresenter;
}

class VehicleWizardView;
class QFrame;

// Ventana principal: barra lateral de módulos y, dentro del área de contenido,
// una pila que alterna entre la rejilla de inventario y el asistente de
// registro.
//
// El menú lateral, las pestañas, el botón "Agregar Vehículo" y el logo vienen
// de mainwindow.ui como widgets promovidos (ver docs/DESIGNER.md). Esta clase
// solo los conecta.
//
// La rejilla es una vista pasiva (IInventoryView): no lee la base ni el disco.
// InventoryPresenter le pide los filtros, lee fuera del hilo de la interfaz y
// le dice qué tarjetas pintar.
class MainWindow : public QMainWindow, public presentation::IInventoryView
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Cómo se crea el asistente de registro. Lo pone la raíz de composición,
    // que es quien tiene el servicio y el TaskRunner que el asistente necesita:
    // esta ventana no sabe armarlos.
    using WizardFactory = std::function<VehicleWizardView *(QWidget *parent)>;
    void setWizardFactory(WizardFactory factory);

    // Quién entró: la barra superior muestra su rol.
    void setSession(const application::SessionDto &session);

    // Conecta los filtros con el presenter de la rejilla y le avisa cuando se
    // registra una unidad.
    void bindInventory(presentation::InventoryPresenter &presenter);

    // --- IInventoryView ---
    application::InventoryFilterDto filter() const override;
    void showVehicles(const QList<application::InventoryItemDto> &vehicles) override;
    void showInventoryMessage(const QString &message) override;
    void scrollToFolio(int folio) override;

private:
    WizardFactory m_wizardFactory;

    // --- Pestañas de contenido ---
    // La línea gris bajo las pestañas es lo único de esa fila que no viene del
    // .ui: el subrayado de la pestaña activa tiene que quedar encima de ella,
    // y eso no se puede expresar con un layout.
    void createNavDivider();

    // Recoloca el divisor de las pestañas. Va por filtro de eventos porque no
    // está en un layout: se superpone a la fila de pestañas.
    bool eventFilter(QObject *watched, QEvent *event) override;
    void layoutNavDivider();

    // --- Inventario ---
    void populateStatusFilter();
    void applyDefaultDateRange();

    // --- Asistente ---
    // Hay a lo más un asistente. "Agregar Vehículo" retoma el que ya exista,
    // con lo capturado intacto, en vez de crear otro. Cambiar de página no lo
    // cierra: solo se destruye cuando él mismo pide volver al inventario, al
    // cancelar o después de registrar.
    void openVehicleWizard();
    void closeVehicleWizard();
    void onVehicleRegistered(int folio);

    void showModulePending(const QString &moduleName);

    Ui::MainWindow *ui;

    QFrame *m_navDivider = nullptr;

    // El asistente en curso, o nulo si no hay ninguno. Mientras no sea nulo,
    // openVehicleWizard() lo vuelve a mostrar en vez de crear otro: esto es
    // lo que garantiza que haya uno solo. Su dueño es mainContentStack, no
    // esta clase. QPointer y no un puntero crudo: si el asistente se
    // destruyera por otro camino que closeVehicleWizard() (al destruirse el
    // stack, por ejemplo), el puntero queda en nulo en vez de colgando.
    QPointer<VehicleWizardView> m_wizard;

    // El presenter de la rejilla. Su dueño es esta ventana (es su hijo).
    QPointer<presentation::InventoryPresenter> m_inventory;
};
#endif // PRESENTATION_VIEWS_SHELL_MAINWINDOW_H
