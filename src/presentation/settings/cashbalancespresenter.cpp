#include "presentation/settings/cashbalancespresenter.h"

#include "application/finance/services/cashledgerservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/settings/icashbalancesview.h"

#include <QDate>

namespace presentation {

CashBalancesPresenter::CashBalancesPresenter(ICashBalancesView &view,
                                             const application::CashLedgerService &service,
                                             TaskRunner &runner, const application::SessionDto &actor,
                                             QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
    , m_actor(actor)
{
}

void CashBalancesPresenter::start()
{
    load();
}

void CashBalancesPresenter::refresh()
{
    if (!m_saving)
        load();
}

bool CashBalancesPresenter::confirmLeave()
{
    if (!m_saving)
        return true;
    m_view.showNotice(QStringLiteral("Espera a que termine de guardarse el movimiento."));
    return false;
}

void CashBalancesPresenter::onFilterChanged()
{
    load(true);
}

void CashBalancesPresenter::load(bool keepMessage)
{
    const int generation = ++m_loadGeneration;
    m_view.setBusy(true);
    const application::CashLedgerService *service = &m_service;
    const application::SessionDto actor = m_actor;
    const std::optional<domain::CashAccount> filter = m_view.movementFilter();
    m_runner.run(
        this, [service, actor, filter] { return service->overview(actor, filter); },
        [this, generation, keepMessage](const application::CashOverviewResult &result) {
            using Status = application::CashOverviewResult::Status;
            if (generation != m_loadGeneration)
                return;
            m_view.setBusy(m_saving);
            switch (result.status) {
            case Status::Loaded:
                m_balances = result.balances;
                m_view.showBalances(m_balances);
                m_view.showMovements(result.movements);
                if (!keepMessage)
                    m_view.showMessage(QString());
                break;
            case Status::Denied:
                m_view.showMessage(QStringLiteral("Tu rol no puede ver los saldos."));
                break;
            case Status::Failed:
                m_view.showMessage(QStringLiteral("No se pudieron leer los saldos: %1")
                                       .arg(result.errorMessage));
                break;
            }
        });
}

qint64 CashBalancesPresenter::balanceOf(domain::CashAccount account) const
{
    for (const application::CashBalanceDto &balance : m_balances) {
        if (balance.account == account)
            return balance.hundredths;
    }
    return 0;
}

void CashBalancesPresenter::onOpeningRequested()
{
    if (m_saving)
        return;
    m_view.showMessage(QString());
    m_view.showNotice(QString());
    m_view.askOpening();
}

void CashBalancesPresenter::onDepositRequested()
{
    if (m_saving)
        return;
    m_view.showMessage(QString());
    m_view.showNotice(QString());
    m_view.askSafeDeposit(m_balances);
}

void CashBalancesPresenter::onWithdrawalRequested()
{
    if (m_saving)
        return;
    m_view.showMessage(QString());
    m_view.showNotice(QString());
    m_view.askSafeWithdrawal(balanceOf(domain::CashAccount::Safe));
}

void CashBalancesPresenter::onSubmitOpening(const application::OpeningBalancesDto &openings)
{
    if (m_saving || !m_view.confirmOpening(openings))
        return;
    const application::CashLedgerService *service = &m_service;
    const application::SessionDto actor = m_actor;
    write([service, actor, openings] { return service->openBalances(openings, actor); },
          QStringLiteral("Se capturaron los saldos iniciales."));
}

void CashBalancesPresenter::onSubmitDeposit(const application::SafeDepositDto &deposit)
{
    const application::CashLedgerService *service = &m_service;
    const application::SessionDto actor = m_actor;
    write([service, actor, deposit] { return service->depositToSafe(deposit, actor); },
          QStringLiteral("Se ingresó el dinero a Caja Fuerte."));
}

void CashBalancesPresenter::onSubmitWithdrawal(const application::SafeWithdrawalDto &withdrawal)
{
    const application::CashLedgerService *service = &m_service;
    const application::SessionDto actor = m_actor;
    const QDate today = QDate::currentDate();
    write([service, actor, withdrawal, today] {
              return service->withdrawFromSafe(withdrawal, actor, today);
          },
          QStringLiteral("Se registró el retiro de Caja Fuerte."));
}

void CashBalancesPresenter::write(std::function<application::CashWriteResult()> work,
                                  const QString &done)
{
    if (m_saving)
        return;
    m_saving = true;
    m_view.setBusy(true);
    m_view.setFormBusy(true);
    m_view.showMessage(QString());
    m_view.showNotice(QString());
    m_runner.run(this, std::move(work), [this, done](const application::CashWriteResult &result) {
        using Status = application::CashWriteResult::Status;
        m_saving = false;
        m_view.setBusy(false);
        m_view.setFormBusy(false);
        switch (result.status) {
        case Status::Saved:
            m_view.closeForm();
            m_view.showNotice(done);
            load();
            return;
        case Status::Rejected:
            // El formulario sigue abierto con su error (también la falta de
            // saldo: dice cuánto hay ahora).
            m_view.showFormErrors(result.validation.errors());
            return;
        case Status::Conflict:
            m_view.closeForm();
            m_view.showMessage(QStringLiteral("Otro administrador ya capturó los saldos iniciales; "
                                              "se cargaron los vigentes."));
            load(true);
            return;
        case Status::Denied:
            m_view.closeForm();
            m_view.showMessage(QStringLiteral("Tu cuenta ya no puede mover los saldos."));
            return;
        case Status::Failed:
            // El formulario sigue abierto con lo capturado, para reintentar.
            m_view.showMessage(QStringLiteral("No se guardó: %1").arg(result.errorMessage));
            return;
        }
    });
}

} // namespace presentation
