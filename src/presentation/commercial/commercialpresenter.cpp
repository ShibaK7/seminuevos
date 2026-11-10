#include "presentation/commercial/commercialpresenter.h"

#include "presentation/commercial/icommercialview.h"
#include "presentation/common/tasks/taskrunner.h"

namespace presentation {

CommercialPresenter::CommercialPresenter(ICommercialView &view,
                                         const application::CommercialService &service,
                                         TaskRunner &runner, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
{
}

void CommercialPresenter::reload()
{
    const int generation = ++*m_generation;
    const application::CommercialFilterDto filter = m_view.filter();
    const application::CommercialService *service = &m_service;
    const std::shared_ptr<std::atomic<int>> latest = m_generation;
    m_runner.run(this,
                 [service, filter, generation, latest] {
                     if (latest->load() != generation)
                         return application::CommercialResultDto();
                     return service->search(filter);
                 },
                 [this, generation](const application::CommercialResultDto &result) {
                     if (generation != m_generation->load())
                         return;
                     if (!result.errorMessage.isEmpty()) {
                         m_view.showCommercialMessage(
                             QStringLiteral("No se pudieron leer las ventas: %1")
                                 .arg(result.errorMessage));
                     } else if (result.sales.isEmpty()) {
                         m_view.showCommercialMessage(
                             QStringLiteral("No hay ventas que coincidan con los filtros seleccionados."));
                     } else {
                         m_view.showSales(result.sales);
                     }
                 });
}

} // namespace presentation
