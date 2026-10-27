#include "presentation/inventory/maintenance/maintenancehistorypresenter.h"

#include "application/inventory/maintenance/services/maintenanceservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/inventory/maintenance/imaintenancehistoryview.h"

#include <algorithm>

namespace presentation {

MaintenanceHistoryPresenter::MaintenanceHistoryPresenter(
    IMaintenanceHistoryView &view, const application::MaintenanceService &service,
    TaskRunner &runner, int folio, domain::UserRole role, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
    , m_folio(folio)
    , m_role(role)
{
}

void MaintenanceHistoryPresenter::start()
{
    loadHistory();
}

void MaintenanceHistoryPresenter::loadHistory()
{
    const int generation = ++m_historyGeneration;
    const application::MaintenanceService *service = &m_service;
    const int folio = m_folio;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, folio, role] { return service->history(folio, role); },
        [this, generation](const application::MaintenanceHistoryResult &result) {
            using Status = application::MaintenanceHistoryResult::Status;
            if (generation != m_historyGeneration)
                return;
            switch (result.status) {
            case Status::Loaded:
                m_items = result.items;
                m_view.showMaintenances(m_items);
                break;
            case Status::Denied:
                // La sección no se ve para este rol: no hay a quién avisarle.
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("No se pudo leer el historial de mantenimientos"),
                                   result.errorMessage);
                break;
            }
        });
}

void MaintenanceHistoryPresenter::onAddMaintenance()
{
    if (m_loadingForm)
        return;
    m_loadingForm = true;
    m_view.setBusy(true);

    // El catálogo se lee en cada clic: es una consulta chica, y así un tipo
    // que se agregue a la base aparece sin volver a abrir el expediente.
    const application::MaintenanceService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, role] { return service->formLookups(role); },
        [this](const application::MaintenanceFormLookupsResult &result) {
            using Status = application::MaintenanceFormLookupsResult::Status;
            m_loadingForm = false;
            m_view.setBusy(false);
            switch (result.status) {
            case Status::Loaded:
                m_view.askNewMaintenance(result.maintenanceTypes);
                break;
            case Status::Denied:
                m_view.showWarning(QStringLiteral("Agregar mantenimiento"),
                                   QStringLiteral("Tu rol no puede agregar mantenimientos."));
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("No se pudieron leer los tipos de mantenimiento"),
                                   result.errorMessage);
                break;
            }
        });
}

void MaintenanceHistoryPresenter::onSubmitMaintenance(
    const application::NewMaintenanceDto &maintenance)
{
    if (m_saving)
        return;
    m_saving = true;
    m_view.setMaintenanceFormBusy(true);

    application::NewMaintenanceDto request = maintenance;
    request.vehicleFolio = m_folio;
    const application::MaintenanceService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, request, role] { return service->add(request, role); },
        [this](const application::MaintenanceAddResult &result) {
            using Status = application::MaintenanceAddResult::Status;
            m_saving = false;
            m_view.setMaintenanceFormBusy(false);
            switch (result.status) {
            case Status::Saved:
                m_view.closeMaintenanceForm();
                if (result.maintenanceCostTotal)
                    emit maintenanceCostChanged(*result.maintenanceCostTotal);
                loadHistory();
                break;
            case Status::Rejected:
                m_view.showMaintenanceFormErrors(result.validation.errors());
                break;
            case Status::Denied:
                m_view.showWarning(QStringLiteral("Agregar mantenimiento"),
                                   QStringLiteral("Tu rol no puede agregar mantenimientos."));
                break;
            case Status::Failed:
                // El formulario sigue abierto con lo capturado, para reintentar.
                m_view.showWarning(QStringLiteral("No se guardó el mantenimiento"),
                                   result.errorMessage);
                break;
            }
        });
}

void MaintenanceHistoryPresenter::onPrintReceipt()
{
    const int id = m_view.selectedMaintenanceId();
    const auto item = std::find_if(m_items.cbegin(), m_items.cend(),
                                   [id](const auto &candidate) { return candidate.id == id; });
    if (item == m_items.cend()) {
        m_view.showWarning(QStringLiteral("Imprimir recibo"),
                           QStringLiteral("Selecciona en la tabla el mantenimiento del que "
                                          "quieres el recibo."));
        return;
    }

    const QString outputPath = m_view.askReceiptPath(
        QStringLiteral("recibo_mantenimiento_folio%1_%2.pdf").arg(m_folio).arg(item->id));
    if (outputPath.isEmpty())
        return;

    const auto outcome = m_service.generateReceipt(*item, outputPath, m_role);
    if (!outcome.ok) {
        m_view.showWarning(QStringLiteral("Error al generar el recibo"), outcome.errorMessage);
        return;
    }
    m_view.openDocument(outputPath);

    // La casilla Recibo solo dice que ya se imprimió una vez: imprimirlo otra
    // vez no tiene nada que anotar.
    if (!item->receiptPrinted)
        markReceiptPrinted(item->id);
}

void MaintenanceHistoryPresenter::onDescriptionEdited(int maintenanceId,
                                                      const QString &description)
{
    const auto item = std::find_if(m_items.cbegin(), m_items.cend(), [maintenanceId](const auto &c) {
        return c.id == maintenanceId;
    });
    if (item == m_items.cend() || item->description == description.trimmed())
        return;
    // Una corrección a la vez: la tabla vuelve a lo guardado.
    if (m_updatingDescription) {
        m_view.showMaintenances(m_items);
        return;
    }
    if (!m_view.confirmDescriptionUpdate()) {
        m_view.showMaintenances(m_items);
        m_view.showInformation(QStringLiteral("Actualización cancelada"));
        return;
    }

    m_updatingDescription = true;
    const application::MaintenanceService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this,
        [service, maintenanceId, description, role] {
            return service->updateDescription(maintenanceId, description, role);
        },
        [this](const application::MaintenanceDescriptionUpdateResult &result) {
            using Status = application::MaintenanceDescriptionUpdateResult::Status;
            m_updatingDescription = false;
            switch (result.status) {
            case Status::Updated:
                // Se relee para pintar la descripción tal como quedó guardada.
                loadHistory();
                m_view.showInformation(QStringLiteral("El registro se actualizó con éxito"));
                return;
            case Status::Rejected:
                m_view.showWarning(QStringLiteral("No se actualizó la descripción"),
                                   result.validation.joinedMessages());
                break;
            case Status::Denied:
                m_view.showWarning(QStringLiteral("No se actualizó la descripción"),
                                   QStringLiteral("Tu rol no puede corregir el historial."));
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("No se actualizó la descripción"),
                                   result.errorMessage);
                break;
            }
            m_view.showMaintenances(m_items);
        });
}

void MaintenanceHistoryPresenter::markReceiptPrinted(int id)
{
    const application::MaintenanceService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, id, role] { return service->markReceiptPrinted(id, role); },
        [this, id](const application::MaintenanceReceiptMarkResult &result) {
            using Status = application::MaintenanceReceiptMarkResult::Status;
            switch (result.status) {
            case Status::Marked:
                for (application::MaintenanceItemDto &item : m_items) {
                    if (item.id == id)
                        item.receiptPrinted = true;
                }
                m_view.checkReceipt(id);
                break;
            case Status::Denied:
                // No llega: generar el recibo ya exigió el mismo rol.
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("Recibo sin anotar"),
                                   QStringLiteral("El recibo se generó, pero no se pudo anotar "
                                                  "como impreso: %1").arg(result.errorMessage));
                break;
            }
        });
}

} // namespace presentation
