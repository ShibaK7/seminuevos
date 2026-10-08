#ifndef PRESENTATION_PRESENTERS_INVENTORYPRESENTER_H
#define PRESENTATION_PRESENTERS_INVENTORYPRESENTER_H

#include "application/dto/inventorydtos.h"

#include <QObject>
#include <QTimer>

#include <atomic>
#include <memory>

namespace application {
class InventoryService;
}

namespace presentation {

class IInventoryView;
class TaskRunner;

// Presenter de la rejilla del inventario. Lee fuera del hilo de la interfaz y
// le dice a la vista qué pintar. Antes la ventana principal corría el SQL y
// cargaba las fotos del disco en el hilo de la interfaz, y la pantalla se
// congelaba en cada cambio de filtro.
class InventoryPresenter final : public QObject
{
    Q_OBJECT

public:
    InventoryPresenter(IInventoryView &view, const application::InventoryService &inventory,
                       TaskRunner &runner, QObject *parent = nullptr);

    // Vuelve a leer ya, con los filtros de la vista. Si llega una respuesta de
    // una lectura anterior, se descarta: solo se pinta la más reciente.
    void reload();

    // Lo que llaman los filtros: espera a que el usuario deje de cambiarlos
    // (teclear una fecha cambia el control con cada dígito) y entonces lee una
    // sola vez.
    void scheduleReload();

    // Una unidad nueva: se recarga y, al llegar, se lleva la lista hasta ella.
    void vehicleRegistered(int folio);

private:
    void onLoaded(int generation, const application::InventoryResultDto &result);

    IInventoryView &m_view;
    const application::InventoryService &m_inventory;
    TaskRunner &m_runner;
    QTimer m_reloadTimer;
    // Cuántas lecturas se han pedido. Cada respuesta trae el número de la
    // suya y solo se pinta la última. Es compartido con el hilo de trabajo:
    // una lectura que ya quedó vieja antes de empezar ni siquiera consulta.
    std::shared_ptr<std::atomic<int>> m_generation = std::make_shared<std::atomic<int>>(0);
    int m_scrollToFolio = -1;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_INVENTORYPRESENTER_H
