#include "presentation/finance/financepresenter.h"

#include "application/finance/services/operationalexpenseservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/finance/ifinanceview.h"

#include <algorithm>

namespace presentation {

namespace {

QString deniedMessage()
{
    return QStringLiteral("Solo el Administrador puede usar Finanzas.");
}

} // namespace

FinancePresenter::FinancePresenter(IFinanceView &view,
                                   const application::OperationalExpenseService &service,
                                   TaskRunner &runner, domain::UserRole role, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
    , m_role(role)
{
}

void FinancePresenter::reload()
{
    const int generation = ++m_listGeneration;
    const application::OperationalExpenseService *service = &m_service;
    const domain::UserRole role = m_role;
    const application::OperationalExpenseFilterDto filter = m_filter;
    m_runner.run(
        this, [service, filter, role] { return service->list(filter, role); },
        [this, generation](const application::OperationalExpenseListResult &result) {
            using Status = application::OperationalExpenseListResult::Status;
            if (generation != m_listGeneration)
                return;
            switch (result.status) {
            case Status::Loaded:
                m_items = result.items;
                m_view.showExpenses(m_items);
                m_view.showYears(result.years);
                m_view.showMonthlySummary(result.summary);
                break;
            case Status::Denied:
                // El menú no se ve para este rol: no hay a quién avisarle.
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("No se pudieron leer los gastos"),
                                   result.errorMessage);
                break;
            }
        });
}

void FinancePresenter::onFilterChanged(const application::OperationalExpenseFilterDto &filter)
{
    m_filter = filter;
    reload();
}

void FinancePresenter::onAddExpense()
{
    if (m_loadingForm)
        return;
    m_loadingForm = true;
    m_view.setBusy(true);

    // El catálogo se lee en cada clic: es una consulta chica, y así un tipo
    // que se agregue a la base aparece sin reiniciar la aplicación.
    const application::OperationalExpenseService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, role] { return service->formLookups(role); },
        [this](const application::ExpenseFormLookupsResult &result) {
            using Status = application::ExpenseFormLookupsResult::Status;
            m_loadingForm = false;
            m_view.setBusy(false);
            switch (result.status) {
            case Status::Loaded:
                // Sin tipos el formulario no se puede llenar (expense_id es
                // NOT NULL): se avisa en vez de abrir un combo vacío.
                if (result.expenseTypes.isEmpty()) {
                    m_view.showWarning(QStringLiteral("No hay tipos de gasto"),
                                       QStringLiteral("El catálogo operational_expense_cat está "
                                                      "vacío. Carga los tipos de gasto en la base "
                                                      "(init-db/15_operational_expense_cat.sql)."));
                    break;
                }
                m_view.askNewExpense(result.expenseTypes);
                break;
            case Status::Denied:
                m_view.showWarning(QStringLiteral("Agregar gasto"), deniedMessage());
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("No se pudieron leer los tipos de gasto"),
                                   result.errorMessage);
                break;
            }
        });
}

void FinancePresenter::onSubmitExpense(const application::NewOperationalExpenseDto &expense)
{
    if (m_saving)
        return;
    m_saving = true;
    m_view.setExpenseFormBusy(true);

    const application::OperationalExpenseService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, expense, role] { return service->add(expense, role); },
        [this](const application::OperationalExpenseAddResult &result) {
            using Status = application::OperationalExpenseAddResult::Status;
            m_saving = false;
            m_view.setExpenseFormBusy(false);
            switch (result.status) {
            case Status::Saved:
                m_view.closeExpenseForm();
                // Se relee para pintar el renglón tal como quedó guardado,
                // con su folio, su descripción y su IVA.
                reload();
                break;
            case Status::Rejected:
                m_view.showExpenseFormErrors(result.validation.errors());
                break;
            case Status::Denied:
                m_view.showWarning(QStringLiteral("No se guardó el gasto"), deniedMessage());
                break;
            case Status::Failed:
                // El formulario sigue abierto con lo capturado, para reintentar.
                m_view.showWarning(QStringLiteral("No se guardó el gasto"), result.errorMessage);
                break;
            }
        });
}

void FinancePresenter::onInvoiceToggled(int folio, bool hasInvoice)
{
    const application::OperationalExpenseItemDto *item = find(folio);
    if (!item)
        return;
    // La casilla volvió a lo guardado: se desmarcó una que se había marcado
    // sin llegar a capturar el número. En la base no cambia nada.
    if (hasInvoice == item->hasInvoice) {
        m_view.showExpenses(m_items);
        return;
    }
    if (!m_view.confirmInvoiceUpdate()) {
        m_view.showExpenses(m_items);
        return;
    }
    if (hasInvoice) {
        // Factura sin número no es válida: se guarda cuando se capture. Esa
        // captura ya no vuelve a preguntar: completa este mismo cambio y no
        // reemplaza ningún número.
        if (item->invoiceNumber.trimmed().isEmpty()) {
            m_view.editInvoiceNumber(folio);
            return;
        }
        saveInvoice(folio, true, item->invoiceNumber);
        return;
    }
    saveInvoice(folio, false, QString());
}

void FinancePresenter::onInvoiceNumberEdited(int folio, const QString &invoiceNumber)
{
    const application::OperationalExpenseItemDto *item = find(folio);
    if (!item)
        return;
    const QString number = invoiceNumber.trimmed();
    if (item->hasInvoice && item->invoiceNumber == number)
        return;
    if (number.isEmpty()) {
        // Se marcó Factura y se salió sin capturar el número: sin él no se
        // puede guardar. O lo captura, o la casilla vuelve a desmarcarse.
        if (!item->hasInvoice) {
            if (m_view.askForMissingInvoiceNumber())
                m_view.editInvoiceNumber(folio);
            else
                m_view.showExpenses(m_items);
            return;
        }
        // Se borró el número de una factura guardada: la regla del dominio
        // lo rechazaría. Quitar la factura es desmarcar la casilla.
        m_view.showWarning(QStringLiteral("No se actualizó la factura"),
                           QStringLiteral("Captura el No. Factura. Si el gasto no tiene "
                                          "factura, desmarca la casilla Factura."));
        m_view.showExpenses(m_items);
        return;
    }
    // Solo se pregunta si se reemplaza un número guardado. El primero, tras
    // marcar la casilla, ya se confirmó con la Factura.
    if (item->hasInvoice && !m_view.confirmInvoiceNumberUpdate()) {
        m_view.showExpenses(m_items);
        return;
    }
    saveInvoice(folio, true, number);
}

void FinancePresenter::onPrintReceipt()
{
    const application::OperationalExpenseItemDto *item = find(m_view.selectedExpenseFolio());
    if (!item) {
        m_view.showWarning(QStringLiteral("Imprimir recibo"),
                           QStringLiteral("Selecciona en la tabla el gasto del que quieres el "
                                          "recibo."));
        return;
    }

    const QString outputPath =
        m_view.askReceiptPath(QStringLiteral("recibo_gasto_folio%1.pdf").arg(item->folio));
    if (outputPath.isEmpty())
        return;

    const auto outcome = m_service.generateReceipt(*item, outputPath, m_role);
    if (!outcome.ok) {
        m_view.showWarning(QStringLiteral("Error al generar el recibo"), outcome.errorMessage);
        return;
    }
    m_view.openDocument(outputPath);
}

void FinancePresenter::saveInvoice(int folio, bool hasInvoice, const QString &invoiceNumber)
{
    // Una corrección a la vez: la tabla vuelve a lo guardado.
    if (m_updatingInvoice) {
        m_view.showExpenses(m_items);
        return;
    }
    m_updatingInvoice = true;

    const application::OperationalExpenseService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this,
        [service, folio, hasInvoice, invoiceNumber, role] {
            return service->updateInvoice(folio, hasInvoice, invoiceNumber, role);
        },
        [this](const application::OperationalExpenseInvoiceUpdateResult &result) {
            using Status = application::OperationalExpenseInvoiceUpdateResult::Status;
            m_updatingInvoice = false;
            switch (result.status) {
            case Status::Updated:
                reload();
                return;
            case Status::Rejected:
                m_view.showWarning(QStringLiteral("No se actualizó la factura"),
                                   result.validation.joinedMessages());
                break;
            case Status::Denied:
                m_view.showWarning(QStringLiteral("No se actualizó la factura"), deniedMessage());
                break;
            case Status::Failed:
                m_view.showWarning(QStringLiteral("No se actualizó la factura"),
                                   result.errorMessage);
                break;
            }
            m_view.showExpenses(m_items);
        });
}

const application::OperationalExpenseItemDto *FinancePresenter::find(int folio) const
{
    const auto item = std::find_if(m_items.cbegin(), m_items.cend(),
                                   [folio](const auto &candidate) { return candidate.folio == folio; });
    return item == m_items.cend() ? nullptr : &*item;
}

} // namespace presentation
