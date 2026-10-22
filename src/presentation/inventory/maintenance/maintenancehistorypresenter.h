#ifndef PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYPRESENTER_H
#define PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYPRESENTER_H

#include "application/inventory/maintenance/dto/maintenancedtos.h"
#include "domain/common/value_objects/enums.h"

#include <QObject>

namespace application {
class MaintenanceService;
}

namespace presentation {

class IMaintenanceHistoryView;
class TaskRunner;

// Presenter del historial de mantenimientos del expediente (US-03.3): pinta
// los mantenimientos del folio, abre el formulario de alta con su catálogo,
// guarda lo capturado (#46) e imprime el recibo del renglón seleccionado
// (#47). Todo lo que toca la base corre fuera del hilo de la interfaz.
//
// Sin widgets (solo QtCore): se prueba con una vista falsa.
class MaintenanceHistoryPresenter final : public QObject
{
    Q_OBJECT

public:
    MaintenanceHistoryPresenter(IMaintenanceHistoryView &view,
                                const application::MaintenanceService &service,
                                TaskRunner &runner, int folio, domain::UserRole role,
                                QObject *parent = nullptr);

    // Lee y pinta el historial del folio. A quien su rol no le deja verlo no
    // se le pinta nada (la sección ni siquiera se ve).
    void start();

    // "+ Agregar Mantenimiento". Mientras se lee el catálogo, otro clic no
    // abre un segundo formulario.
    void onAddMaintenance();
    // Aceptar en el formulario. El folio lo pone el presenter: es el del
    // expediente. El mantenimiento nace con su recibo sin imprimir.
    void onSubmitMaintenance(const application::NewMaintenanceDto &maintenance);
    // "Imprimir": el recibo del renglón seleccionado, con los datos tal como
    // se leyeron de la base (no el texto de la tabla). La primera vez anota
    // en la base que ya se imprimió y marca su casilla Recibo; se puede volver
    // a imprimir cuantas veces se quiera.
    void onPrintReceipt();
    // Se terminó de editar la descripción de un renglón en la tabla (Enter o
    // al salir de la celda). Si cambió, pregunta si se guarda: al aceptar la
    // guarda en la base, y al cancelar, o si no se pudo guardar, la tabla
    // vuelve a lo guardado. Avisa cómo terminó.
    void onDescriptionEdited(int maintenanceId, const QString &description);

signals:
    // Se guardó un mantenimiento y la operación de la unidad acumula ahora
    // este total. La raíz lo conecta con el campo "Mantenimientos" del
    // expediente.
    void maintenanceCostChanged(double total);

private:
    void loadHistory();
    // Anota en la base que ya se imprimió el recibo `id` y, si queda, marca
    // su casilla.
    void markReceiptPrinted(int id);

    IMaintenanceHistoryView &m_view;
    const application::MaintenanceService &m_service;
    TaskRunner &m_runner;
    const int m_folio;
    const domain::UserRole m_role;
    bool m_loadingForm = false;
    bool m_saving = false;
    bool m_updatingDescription = false;
    // Cada lectura del historial lleva un número; solo se pinta la última.
    int m_historyGeneration = 0;
    // Lo último que se pintó, para el recibo.
    QList<application::MaintenanceItemDto> m_items;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_MAINTENANCE_MAINTENANCEHISTORYPRESENTER_H
