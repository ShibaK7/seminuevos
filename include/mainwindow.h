#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

struct VehicleInventoryFilter;
class SidebarMenuItem;
class NavTabItem;
class OutlineButton;
class QDateEdit;
class QFrame;
class QLabel;

// Ventana principal: barra lateral de módulos y, dentro del área de contenido,
// una pila que alterna entre la rejilla de inventario y el asistente de
// registro.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    // --- Barra lateral ---
    // Los renglones del menú no vienen del .ui: SidebarMenuItem recibe título
    // e ícono por constructor, y el Designer solo sabe instanciar widgets con
    // un constructor de (QWidget *).
    void buildSidebarMenu();

    // --- Pestañas de contenido ---
    // Mismo motivo que la barra lateral: NavTabItem recibe el título por
    // constructor y el Designer no sabe instanciarlo.
    void buildNavTabs();

    // Recoloca el divisor y los iconos de calendario. Van por filtro de
    // eventos porque ninguno está en un layout: se superponen a su widget.
    bool eventFilter(QObject *watched, QEvent *event) override;
    void layoutNavDivider();

    // Cuelga el icono de calendario dentro del campo de fecha. Va como QLabel
    // y no como `image` del subcontrol ::down-arrow porque por QSS no llegaba
    // a dibujarse -- ver el comentario de esa regla en global-style.qss.
    void attachCalendarIcon(QDateEdit *dateEdit);
    void layoutCalendarIcon(QDateEdit *dateEdit);

    // --- Inventario ---
    void populateStatusFilter();
    void applyDefaultDateRange();
    VehicleInventoryFilter currentFilter() const;
    void reloadInventory();
    void showInventoryMessage(const QString &message);
    void scrollToFolio(int folio);

    // --- Asistente ---
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

    // Icono por campo de fecha, indexado por el campo al que se superpone: el
    // filtro de eventos recibe el QDateEdit y necesita llegar a su icono.
    QHash<QDateEdit *, QLabel *> m_calendarIcons;
};
#endif // MAINWINDOW_H
