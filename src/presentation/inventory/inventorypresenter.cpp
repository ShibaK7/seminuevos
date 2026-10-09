#include "presentation/inventory/inventorypresenter.h"

#include "application/inventory/services/inventoryservice.h"
#include "presentation/inventory/iinventoryview.h"
#include "presentation/common/tasks/taskrunner.h"

#include <utility>

namespace presentation {

namespace {

// Cuánto se espera después del último cambio de filtro para leer.
constexpr int kReloadDelayMs = 250;

} // namespace

InventoryPresenter::InventoryPresenter(IInventoryView &view,
                                       const application::InventoryService &inventory,
                                       TaskRunner &runner, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_inventory(inventory)
    , m_runner(runner)
{
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(kReloadDelayMs);
    connect(&m_reloadTimer, &QTimer::timeout, this, &InventoryPresenter::reload);
}

void InventoryPresenter::reload()
{
    m_reloadTimer.stop();
    const int generation = ++*m_generation;
    const application::InventoryFilterDto filter = m_view.filter();
    const application::InventoryService *inventory = &m_inventory;
    const std::shared_ptr<std::atomic<int>> latest = m_generation;
    m_runner.run(this,
                 [inventory, filter, generation, latest] {
                     // Si ya se pidió otra lectura mientras esta esperaba su
                     // turno, no vale la pena consultar: su respuesta se
                     // descartaría de todos modos.
                     if (latest->load() != generation)
                         return application::InventoryResultDto();
                     return inventory->search(filter);
                 },
                 [this, generation](const application::InventoryResultDto &result) {
                     onLoaded(generation, result);
                 });
}

void InventoryPresenter::scheduleReload()
{
    // start() reinicia la cuenta si ya corría.
    m_reloadTimer.start();
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
    if (generation != m_generation->load())
        return;

    // La unidad a la que había que llevar la lista se busca solo en esta
    // respuesta: si no llegó, una recarga posterior no debe saltar a ella.
    const int scrollTo = std::exchange(m_scrollToFolio, -1);

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
    if (scrollTo >= 0)
        m_view.scrollToFolio(scrollTo);
}

} // namespace presentation
