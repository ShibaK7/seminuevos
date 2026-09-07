#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

struct VehicleInventoryFilter;

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
};
#endif // MAINWINDOW_H
