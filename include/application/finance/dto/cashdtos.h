#ifndef APPLICATION_FINANCE_DTO_CASHDTOS_H
#define APPLICATION_FINANCE_DTO_CASHDTOS_H

#include "domain/common/value_objects/validationresult.h"
#include "domain/finance/value_objects/cashmovement.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace application {

// El saldo vigente de una cuenta, en centésimos.
struct CashBalanceDto
{
    domain::CashAccount account = domain::CashAccount::PettyCash;
    qint64 hundredths = 0;
    // Si ya se capturó su saldo inicial (Caja Fuerte nace abierta).
    bool opened = false;
    // La de su último movimiento; inválida si nunca se movió.
    QDateTime updatedAt;
};

// Un renglón de la bitácora, como se lista en Configuraciones.
struct CashMovementDto
{
    int id = -1;
    domain::CashAccount account = domain::CashAccount::PettyCash;
    // Con signo: positivo entró, negativo salió.
    qint64 hundredths = 0;
    domain::CashMovementKind kind = domain::CashMovementKind::Opening;
    QString concept;
    QDate date;
    QDateTime createdAt;
    // Quien lo capturó en Configuraciones; vacío si lo causó una operación.
    QString userName;
};

// Los saldos (siempre los tres, en el orden de pantalla) y los movimientos
// más recientes.
struct CashOverviewResult
{
    enum class Status { Loaded, Denied, Failed };
    Status status = Status::Failed;
    QList<CashBalanceDto> balances;
    QList<CashMovementDto> movements;
    QString errorMessage; // en Failed
};

// Los saldos iniciales, que se capturan una sola vez.
struct OpeningBalancesDto
{
    qint64 pettyCashHundredths = 0;
    qint64 bankHundredths = 0;
};

// Ingreso a Caja Fuerte desde Caja Chica o Cantidad Banco.
struct SafeDepositDto
{
    domain::CashAccount source = domain::CashAccount::PettyCash;
    qint64 hundredths = 0;
};

// Retiro de Caja Fuerte: cuándo salió, el concepto y cuánto.
struct SafeWithdrawalDto
{
    QDate date;
    QString concept;
    qint64 hundredths = 0;
};

struct CashWriteResult
{
    enum class Status {
        Saved,
        Rejected, // un campo no pasa (incluida la falta de saldo); ver validation
        Denied,   // el rol, o la cuenta ya no es Administrador activo
        Conflict, // los saldos iniciales ya se habían capturado
        Failed,   // falló la base
    };
    Status status = Status::Failed;
    domain::ValidationResult validation;
    QString errorMessage;
};

} // namespace application

#endif // APPLICATION_FINANCE_DTO_CASHDTOS_H
