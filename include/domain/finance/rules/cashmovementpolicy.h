#ifndef DOMAIN_FINANCE_RULES_CASHMOVEMENTPOLICY_H
#define DOMAIN_FINANCE_RULES_CASHMOVEMENTPOLICY_H

#include "domain/common/value_objects/validationresult.h"
#include "domain/finance/value_objects/cashaccount.h"

#include <QDate>
#include <QString>

namespace domain {

// Lo que el Administrador captura a mano sobre las cuentas: el saldo inicial,
// el ingreso a Caja Fuerte y el retiro de ella. Los montos van en centésimos.
// Nada puede sacar más de lo que hay en existencia.
class CashMovementPolicy
{
public:
    // El mayor movimiento que cabe en NUMERIC(12, 2), en centésimos.
    static constexpr qint64 kMaxHundredths = Q_INT64_C(999999999999);
    static constexpr int kMaxConceptLength = 200;

    // Las claves con que la pantalla marca sus campos.
    static QString openingField(CashAccount account);
    static QString sourceField();
    static QString amountField();
    static QString dateField();
    static QString conceptField();

    // Saldo inicial de Caja Chica o Cantidad Banco: cero o más. Caja Fuerte
    // no tiene: empieza en cero.
    static void checkOpening(CashAccount account, qint64 hundredths, ValidationResult &result);

    // Ingreso a Caja Fuerte: desde Caja Chica o Cantidad Banco, por más de
    // cero y no más de lo que hay en el origen.
    static void checkSafeDeposit(CashAccount source, qint64 hundredths, qint64 available,
                                 ValidationResult &result);

    // Retiro de Caja Fuerte: cuándo salió (no después de hoy), el concepto y
    // cuánto (más de cero y no más de lo que hay).
    static void checkSafeWithdrawal(const QDate &date, const QDate &today, const QString &concept,
                                    qint64 hundredths, qint64 available, ValidationResult &result);

    // "Caja Chica solo tiene $1,500.00 disponibles."
    static QString insufficientFundsMessage(CashAccount account, qint64 available);

private:
    static void checkWithin(qint64 hundredths, CashAccount from, qint64 available,
                            ValidationResult &result);
};

// $12,345.67, como se lee en un recibo.
QString formatPesos(qint64 hundredths);

} // namespace domain

#endif // DOMAIN_FINANCE_RULES_CASHMOVEMENTPOLICY_H
