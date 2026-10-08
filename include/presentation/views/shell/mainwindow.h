#ifndef PRESENTATION_VIEWS_SHELL_MAINWINDOW_H
#define PRESENTATION_VIEWS_SHELL_MAINWINDOW_H

#include "application/auth/dto/authdtos.h"

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

class InventoryView;
class VehicleWizardView;

// Ventana principal: la cáscara. Barra lateral de módulos, barra superior con
// el rol y, dentro del área de contenido, una pila que alterna entre la página
// del inventario (InventoryView, con su propio .ui) y el asistente de registro.
//
// El menú lateral y el logo vienen de mainwindow.ui como widgets promovidos
// ("Promover a…"). Esta clase solo los conecta y decide qué página se
// ve; lo que pasa dentro de cada página es de esa página y de su presenter.
class MainWindow : public QMainWindow
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

    // La página del inventario, para que la raíz de composición le arme su
    // presenter.
    InventoryView &inventoryView();

    // Conecta la página del inventario con su presenter, y a este con los
    // registros que haga el asistente.
    void bindInventory(presentation::InventoryPresenter &presenter);

private:
    WizardFactory m_wizardFactory;

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
