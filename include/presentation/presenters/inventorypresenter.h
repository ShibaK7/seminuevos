#ifndef PRESENTATION_PRESENTERS_INVENTORYPRESENTER_H
#define PRESENTATION_PRESENTERS_INVENTORYPRESENTER_H

#include "application/dto/inventorydtos.h"

#include <QObject>

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

    // Vuelve a leer con los filtros de la vista. Si llega una respuesta de
    // una lectura anterior (el usuario cambió el filtro dos veces seguidas),
    // se descarta: solo se pinta la más reciente.
    void reload();

    // Una unidad nueva: se recarga y, al llegar, se lleva la lista hasta ella.
    void vehicleRegistered(int folio);

private:
    void onLoaded(int generation, const application::InventoryResultDto &result);

    IInventoryView &m_view;
    const application::InventoryService &m_inventory;
    TaskRunner &m_runner;
    // Cuántas lecturas se han pedido; cada respuesta trae el número de la
    // suya y solo se pinta la última.
    int m_generation = 0;
    int m_scrollToFolio = -1;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_INVENTORYPRESENTER_H
