#ifndef APPLICATION_FINANCE_SERVICES_CASHLEDGERSERVICE_H
#define APPLICATION_FINANCE_SERVICES_CASHLEDGERSERVICE_H

#include "application/auth/dto/authdtos.h"
#include "application/finance/dto/cashdtos.h"
#include "application/finance/ports/cashledger.h"

#include <optional>

namespace application {

// Caso de uso "consultar y mover las cuentas de dinero" en Configuraciones:
// ver los saldos y la bitácora, capturar una vez los saldos iniciales, y
// llenar o vaciar Caja Fuerte. Solo el Administrador (SettingsAccess).
//
// Valida contra los saldos que acaba de leer (nada saca más de lo que hay); si
// entre leer y guardar otra sesión gastó ese dinero, el puerto no aplica el
// movimiento y el resultado es el mismo rechazo con el saldo vigente.
//
// Solo guarda una referencia a su puerto: se puede llamar desde el hilo de
// trabajo. Hace E/S: correrlo fuera del hilo de la interfaz.
class CashLedgerService final
{
public:
    static constexpr int kMovementsShown = 100;

    explicit CashLedgerService(CashLedger &ledger);

    CashOverviewResult overview(const SessionDto &actor,
                                std::optional<domain::CashAccount> account = std::nullopt) const;

    CashWriteResult openBalances(const OpeningBalancesDto &openings, const SessionDto &actor) const;
    CashWriteResult depositToSafe(const SafeDepositDto &deposit, const SessionDto &actor) const;
    // `today` acota la fecha del retiro (no puede ser posterior).
    CashWriteResult withdrawFromSafe(const SafeWithdrawalDto &withdrawal, const SessionDto &actor,
                                     const QDate &today) const;

private:
    // Lee los saldos; nullopt (y el resultado de falla en `failed`) si no se
    // pudo.
    std::optional<QList<CashBalanceDto>> readBalances(CashWriteResult &failed) const;
    CashWriteResult fromOutcome(const CashLedger::WriteOutcome &outcome) const;

    CashLedger &m_ledger;
};

} // namespace application

#endif // APPLICATION_FINANCE_SERVICES_CASHLEDGERSERVICE_H
