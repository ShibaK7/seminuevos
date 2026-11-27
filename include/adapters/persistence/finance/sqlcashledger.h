#ifndef ADAPTERS_PERSISTENCE_FINANCE_SQLCASHLEDGER_H
#define ADAPTERS_PERSISTENCE_FINANCE_SQLCASHLEDGER_H

#include "application/finance/ports/cashledger.h"

#include <QSqlDatabase>

#include <functional>

class ConnectionPool;

// Adaptador del puerto CashLedger sobre cash_accounts y cash_movements. Cada
// escritura corre en una transacción que primero comprueba que quien actúa
// siga siendo Administrador activo y luego aplica sus movimientos con
// cashledger::applyMovements (solo si hay saldo).
class SqlCashLedger final : public application::CashLedger
{
public:
    explicit SqlCashLedger(ConnectionPool &pool);

    QList<application::CashBalanceDto> balances(QString *error) override;
    QList<application::CashMovementDto> recentMovements(std::optional<domain::CashAccount> account,
                                                        int limit, QString *error) override;
    WriteOutcome open(const application::OpeningBalancesDto &openings, int actorId) override;
    WriteOutcome depositToSafe(const application::SafeDepositDto &deposit, int actorId) override;
    WriteOutcome withdrawFromSafe(const application::SafeWithdrawalDto &withdrawal, int actorId) override;

private:
    // La transacción común: actor, `write` y commit. `write` devuelve el
    // resultado (Ok para confirmar).
    WriteOutcome inAdministratorTransaction(int actorId,
                                            const std::function<WriteOutcome(QSqlDatabase &)> &write);

    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_FINANCE_SQLCASHLEDGER_H
