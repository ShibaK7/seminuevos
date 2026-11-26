#ifndef ADAPTERS_PERSISTENCE_FINANCE_CASHLEDGERWRITER_H
#define ADAPTERS_PERSISTENCE_FINANCE_CASHLEDGERWRITER_H

#include "domain/finance/value_objects/cashmovement.h"

#include <QList>
#include <QString>

#include <optional>

class QSqlDatabase;

// Aplica movimientos a las cuentas de dinero dentro de una transacción que ya
// abrió quien llama: el alta de una unidad, una venta, un gasto. Así la
// operación y su movimiento se guardan juntos o no se guarda ninguno.
//
// Cada movimiento:
//   UPDATE cash_accounts SET balance = balance + :monto
//    WHERE account = :cuenta AND balance + :monto >= 0
// y su renglón en cash_movements. Si el UPDATE no toca nada, en la cuenta no
// hay saldo: se detiene y lo reporta, y quien llama deshace su transacción.
// El candado del renglón de la cuenta ordena a dos sesiones que la muevan a la
// vez, y CashFlowRules::inApplyOrder fija el orden entre cuentas.
namespace cashledger {

// Con qué se relaciona el movimiento (todo opcional).
struct References
{
    std::optional<int> userId;
    std::optional<int> vehicleFolio;
    std::optional<int> saleFolio;
    std::optional<int> transferId;
};

struct ApplyOutcome
{
    enum class Status { Applied, Insufficient, Failed };
    Status status = Status::Failed;
    // En Insufficient: la cuenta sin saldo y lo que tiene, en centésimos.
    domain::CashAccount account = domain::CashAccount::PettyCash;
    qint64 available = 0;
    QString errorMessage; // en Failed
};

ApplyOutcome applyMovements(QSqlDatabase &db, const QList<domain::CashMovement> &movements,
                            const References &references = {});

// Un número nuevo para unir las dos patas de un traspaso; nullopt (y la
// causa en `error`) si falla.
std::optional<int> nextTransferId(QSqlDatabase &db, QString *error);

// El saldo de una cuenta en centésimos, leído en `db`.
std::optional<qint64> balanceOf(QSqlDatabase &db, domain::CashAccount account, QString *error);

} // namespace cashledger

#endif // ADAPTERS_PERSISTENCE_FINANCE_CASHLEDGERWRITER_H
