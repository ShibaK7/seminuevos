#ifndef DOMAIN_FINANCE_RULES_CASHFLOWRULES_H
#define DOMAIN_FINANCE_RULES_CASHFLOWRULES_H

#include "domain/common/value_objects/enums.h"
#include "domain/finance/value_objects/cashmovement.h"

#include <QList>

namespace domain {

// Los términos de una compra que deciden si mueve dinero.
struct PurchaseTerms
{
    AcquisitionType acquisitionType = AcquisitionType::Adquisicion;
    PaymentType paymentType = PaymentType::Contado;
    PaymentMethod paymentMethod = PaymentMethod::Transferencia;
    qint64 priceHundredths = 0;

    bool operator==(const PurchaseTerms &other) const;
    bool operator!=(const PurchaseTerms &other) const { return !(*this == other); }
};

// Qué mueve cada operación de los módulos en Caja Chica y Cantidad Banco. Es
// el único lugar con esta regla: el alta, el expediente, las ventas y (en
// Finanzas) los gastos y mantenimientos la piden aquí, y su adaptador aplica
// lo que devuelve dentro de la misma transacción que guarda la operación.
//
// Cada función devuelve los movimientos (vacío si la operación no mueve
// dinero); el concepto y la fecha los pone quien llama, que sabe el folio.
class CashFlowRules
{
public:
    // Una compra de contado saca el precio de la cuenta de su método. La
    // consignación no se paga al recibirla y la compra a crédito todavía no
    // mueve saldo (se pagará con un flujo que aún no existe).
    static QList<CashMovement> forPurchase(const PurchaseTerms &terms, const QString &concept,
                                           const QDate &date);

    // El expediente corrigió una compra: lo que ya salió por los términos de
    // antes vuelve, y sale lo de los nuevos. Se neta por cuenta (bajar el
    // precio de 100,000 a 90,000 en la misma cuenta es UNA entrada de
    // 10,000). Si los términos no cambiaron, no hay movimiento.
    static QList<CashMovement> forPurchaseCorrection(const PurchaseTerms &before,
                                                     const PurchaseTerms &after,
                                                     const QString &concept, const QDate &date);

    // Una venta de contado mete el total en la cuenta de su método.
    static QList<CashMovement> forCashSale(PaymentMethod method, qint64 totalHundredths,
                                           const QString &concept, const QDate &date);

    // Una venta a crédito mete el enganche en la cuenta del método con que se
    // pagó (sin enganche, nada). Lo financiado entra con los abonos.
    static QList<CashMovement> forCreditSale(PaymentMethod downPaymentMethod,
                                             qint64 downPaymentHundredths, const QString &concept,
                                             const QDate &date);

    // Lo que paga Finanzas: un mantenimiento o un gasto operativo sale de la
    // cuenta de su método.
    static QList<CashMovement> forMaintenance(PaymentMethod method, qint64 costHundredths,
                                              const QString &concept, const QDate &date);
    static QList<CashMovement> forExpense(PaymentMethod method, qint64 totalHundredths,
                                          const QString &concept, const QDate &date);

    // El orden en que se aplican: por cuenta, siempre el mismo, para que dos
    // sesiones que mueven las mismas cuentas no se esperen una a la otra en
    // orden cruzado; y en una misma cuenta, primero lo que entra, para no
    // fallar por falta de saldo a medio camino.
    static QList<CashMovement> inApplyOrder(QList<CashMovement> movements);
};

} // namespace domain

#endif // DOMAIN_FINANCE_RULES_CASHFLOWRULES_H
