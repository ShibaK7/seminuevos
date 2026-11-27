#ifndef DOMAIN_FINANCE_VALUE_OBJECTS_CASHMOVEMENT_H
#define DOMAIN_FINANCE_VALUE_OBJECTS_CASHMOVEMENT_H

#include "domain/finance/value_objects/cashaccount.h"

#include <QDate>
#include <QString>

#include <optional>

namespace domain {

// Por qué se movió el dinero.
enum class CashMovementKind {
    Opening,            // el saldo inicial que se captura una vez
    Purchase,           // compra de contado de una unidad
    PurchaseCorrection, // el expediente cambió el precio, el tipo o el método
    Sale,               // venta de contado
    DownPayment,        // enganche de una venta a crédito
    Transfer,           // ingreso a Caja Fuerte desde Caja Chica o Banco
    Withdrawal,         // retiro de Caja Fuerte
    Maintenance,        // pago de un mantenimiento
    Expense,            // gasto operativo
};

QString toDbString(CashMovementKind kind);
std::optional<CashMovementKind> cashMovementKindFromDb(const QString &raw);

// Una entrada o salida de dinero en una cuenta. El monto va en centésimos y
// con signo: positivo entra, negativo sale; nunca cero (un movimiento sin
// dinero no es movimiento). Se crea con inflow() u outflow() para que el
// signo no dependa de quien lo arma.
class CashMovement
{
public:
    // `hundredths` es la magnitud (mayor que cero).
    static CashMovement inflow(CashAccount account, qint64 hundredths, CashMovementKind kind,
                               const QString &concept, const QDate &date);
    static CashMovement outflow(CashAccount account, qint64 hundredths, CashMovementKind kind,
                                const QString &concept, const QDate &date);

    CashAccount account() const;
    // Con signo: positivo entra, negativo sale.
    qint64 hundredths() const;
    bool isInflow() const;
    CashMovementKind kind() const;
    QString concept() const;
    QDate date() const;

    bool operator==(const CashMovement &other) const;
    bool operator!=(const CashMovement &other) const { return !(*this == other); }

private:
    CashMovement(CashAccount account, qint64 hundredths, CashMovementKind kind, const QString &concept,
                 const QDate &date);

    CashAccount m_account;
    qint64 m_hundredths;
    CashMovementKind m_kind;
    QString m_concept;
    QDate m_date;
};

// Un importe en pesos (double, como lo traen los DTO de los módulos) en
// centésimos exactos.
qint64 toHundredths(double pesos);

} // namespace domain

#endif // DOMAIN_FINANCE_VALUE_OBJECTS_CASHMOVEMENT_H
