#ifndef PRESENTATION_VIEWS_SHELL_MAINWINDOW_H
#define PRESENTATION_VIEWS_SHELL_MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

struct VehicleInventoryFilter;
class SidebarMenuItem;
class NavTabItem;
class OutlineButton;
class VehicleWizardView;
class QFrame;

// Ventana principal: barra lateral de módulos y, dentro del área de contenido,
// una pila que alterna entre la rejilla de inventario y el asistente de
// registro.
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

private:
    WizardFactory m_wizardFactory;
    // --- Barra lateral ---
    // Los renglones del menú no vienen del .ui: SidebarMenuItem recibe título
    // e ícono por constructor, y el Designer solo sabe instanciar widgets con
    // un constructor de (QWidget *).
    void buildSidebarMenu();

    // --- Pestañas de contenido ---
    // Mismo motivo que la barra lateral: NavTabItem recibe el título por
    // constructor y el Designer no sabe instanciarlo.
    void buildNavTabs();

    // Recoloca el divisor de las pestañas. Va por filtro de eventos porque no
    // está en un layout: se superpone a la fila de pestañas.
    bool eventFilter(QObject *watched, QEvent *event) override;
    void layoutNavDivider();

    // --- Inventario ---
    void populateStatusFilter();
    void applyDefaultDateRange();
    VehicleInventoryFilter currentFilter() const;
    void reloadInventory();
    void showInventoryMessage(const QString &message);
    void scrollToFolio(int folio);

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

    // Propiedad del layout de la barra lateral, no de esta clase: se guardan
    // los punteros solo para poder conectarlos y consultar su estado.
    SidebarMenuItem *m_inventoryItem = nullptr;
    SidebarMenuItem *m_commercialItem = nullptr;
    SidebarMenuItem *m_financeItem = nullptr;
    SidebarMenuItem *m_reportItem = nullptr;

    NavTabItem *m_acquisitionTab = nullptr;
    NavTabItem *m_consignmentTab = nullptr;
    QFrame *m_navDivider = nullptr;
    OutlineButton *m_addVehicleButton = nullptr;

    // El asistente en curso, o nulo si no hay ninguno. Mientras no sea nulo,
    // openVehicleWizard() lo vuelve a mostrar en vez de crear otro: esto es
    // lo que garantiza que haya uno solo. Su dueño es mainContentStack, no
    // esta clase. QPointer y no un puntero crudo: si el asistente se
    // destruyera por otro camino que closeVehicleWizard() (al destruirse el
    // stack, por ejemplo), el puntero queda en nulo en vez de colgando.
    QPointer<VehicleWizardView> m_wizard;
};
#endif // PRESENTATION_VIEWS_SHELL_MAINWINDOW_H
