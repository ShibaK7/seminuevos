#ifndef PRESENTATION_SETTINGS_CASHBALANCESPRESENTER_H
#define PRESENTATION_SETTINGS_CASHBALANCESPRESENTER_H

#include "application/auth/dto/authdtos.h"
#include "application/finance/dto/cashdtos.h"

#include <QObject>

#include <functional>

namespace application {
class CashLedgerService;
}

namespace presentation {

class ICashBalancesView;
class TaskRunner;

// Presenter de la tarjeta de saldos de Configuraciones: lee los saldos y los
// movimientos, captura una vez los saldos iniciales, y llena o vacía Caja
// Fuerte. Lo que toca la base corre fuera del hilo de la interfaz.
//
// Sin widgets (solo QtCore): se prueba con una vista falsa.
class CashBalancesPresenter final : public QObject
{
    Q_OBJECT

public:
    CashBalancesPresenter(ICashBalancesView &view, const application::CashLedgerService &service,
                          TaskRunner &runner, const application::SessionDto &actor,
                          QObject *parent = nullptr);

    void start();
    // Vuelve a leer, salvo mientras guarda.
    void refresh();
    // Si se puede salir de la página: mientras guarda, no.
    bool confirmLeave();

    void onFilterChanged();
    void onOpeningRequested();
    void onDepositRequested();
    void onWithdrawalRequested();
    void onSubmitOpening(const application::OpeningBalancesDto &openings);
    void onSubmitDeposit(const application::SafeDepositDto &deposit);
    void onSubmitWithdrawal(const application::SafeWithdrawalDto &withdrawal);

private:
    // Una lectura que sale bien borra el error de una anterior, salvo con
    // `keepMessage`: la que lanza un conflicto deja visible su aviso.
    void load(bool keepMessage = false);
    void write(std::function<application::CashWriteResult()> work, const QString &done);
    qint64 balanceOf(domain::CashAccount account) const;

    ICashBalancesView &m_view;
    const application::CashLedgerService &m_service;
    TaskRunner &m_runner;
    const application::SessionDto m_actor;
    QList<application::CashBalanceDto> m_balances;
    bool m_saving = false;
    // Cada lectura lleva un número; solo se pinta la última.
    int m_loadGeneration = 0;
};

} // namespace presentation

#endif // PRESENTATION_SETTINGS_CASHBALANCESPRESENTER_H
