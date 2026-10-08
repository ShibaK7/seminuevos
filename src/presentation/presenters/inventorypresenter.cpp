#include "presentation/presenters/inventorypresenter.h"

#include "application/services/inventoryservice.h"
#include "presentation/presenters/iinventoryview.h"
#include "presentation/tasks/taskrunner.h"

namespace presentation {

InventoryPresenter::InventoryPresenter(IInventoryView &view,
                                       const application::InventoryService &inventory,
                                       TaskRunner &runner, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_inventory(inventory)
    , m_runner(runner)
{
}

void InventoryPresenter::reload()
{
    const int generation = ++m_generation;
    const application::InventoryFilterDto filter = m_view.filter();
    const application::InventoryService *inventory = &m_inventory;
    m_runner.run(this, [inventory, filter] { return inventory->search(filter); },
                 [this, generation](const application::InventoryResultDto &result) {
                     onLoaded(generation, result);
                 });
}

void InventoryPresenter::vehicleRegistered(int folio)
{
    m_scrollToFolio = folio;
    reload();
}

void InventoryPresenter::onLoaded(int generation, const application::InventoryResultDto &result)
{
    // Una respuesta vieja llegó después de que se pidió otra: pintarla
    // mostraría resultados de un filtro que ya no está puesto.
    if (generation != m_generation)
        return;

    if (!result.errorMessage.isEmpty()) {
        m_view.showInventoryMessage(
            QStringLiteral("No se pudo leer el inventario: %1").arg(result.errorMessage));
        return;
    }
    if (result.vehicles.isEmpty()) {
        m_view.showInventoryMessage(
            QStringLiteral("No hay vehículos que coincidan con los filtros seleccionados."));
        return;
    }

    m_view.showVehicles(result.vehicles);
    if (m_scrollToFolio >= 0) {
        m_view.scrollToFolio(m_scrollToFolio);
        m_scrollToFolio = -1;
    }
}

} // namespace presentation
