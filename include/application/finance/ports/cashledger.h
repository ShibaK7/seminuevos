#ifndef APPLICATION_FINANCE_PORTS_CASHLEDGER_H
#define APPLICATION_FINANCE_PORTS_CASHLEDGER_H

#include "application/finance/dto/cashdtos.h"

#include <QList>
#include <QString>

#include <optional>

namespace application {

// Puerto: las cuentas de dinero (Caja Chica, Cantidad Banco y Caja Fuerte) y
// su bitácora. Lo que mueven las operaciones de los módulos lo aplican sus
// propios adaptadores dentro de su transacción; por aquí pasa solo lo que el
// Administrador captura a mano. Cada escritura es todo o nada, aplica los
// movimientos solo si hay saldo, y vuelve a comprobar en la base que quien
// actúa siga siendo Administrador activo.
class CashLedger
{
public:
    struct WriteOutcome
    {
        enum class Status {
            Ok,
            Insufficient,    // la cuenta `account` ya no tiene saldo; ver `available`
            AlreadyOpened,   // otro administrador ya capturó los saldos iniciales
            ActorNotAllowed, // quien escribe ya no es Administrador activo
            Failed,
        };
        Status status = Status::Failed;
        domain::CashAccount account = domain::CashAccount::PettyCash;
        qint64 available = 0;
        QString errorMessage; // en Failed
    };

    virtual ~CashLedger() = default;

    // Las tres cuentas, en el orden de pantalla. Si falla devuelve vacío y deja
    // la causa en `error`.
    virtual QList<CashBalanceDto> balances(QString *error) = 0;

    // Los más recientes primero; de una sola cuenta si se indica.
    virtual QList<CashMovementDto> recentMovements(std::optional<domain::CashAccount> account,
                                                   int limit, QString *error) = 0;

    // Abre Caja Chica y Cantidad Banco con su saldo inicial, las dos o
    // ninguna.
    virtual WriteOutcome open(const OpeningBalancesDto &openings, int actorId) = 0;

    // Traspaso: sale del origen y entra a Caja Fuerte.
    virtual WriteOutcome depositToSafe(const SafeDepositDto &deposit, int actorId) = 0;

    virtual WriteOutcome withdrawFromSafe(const SafeWithdrawalDto &withdrawal, int actorId) = 0;

protected:
    CashLedger() = default;
    CashLedger(const CashLedger &) = default;
    CashLedger &operator=(const CashLedger &) = default;
};

} // namespace application

#endif // APPLICATION_FINANCE_PORTS_CASHLEDGER_H
