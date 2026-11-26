// Qué mueve cada operación en Caja Chica, Cantidad Banco y Caja Fuerte, y lo
// que el Administrador puede capturar a mano sobre ellas.

#include "domain/finance/rules/cashflowrules.h"
#include "domain/finance/rules/cashmovementpolicy.h"

#include <QtTest>

using namespace domain;

namespace {

const QDate kToday(2026, 11, 26);

PurchaseTerms cashPurchase(qint64 price, PaymentMethod method = PaymentMethod::Efectivo)
{
    PurchaseTerms terms;
    terms.acquisitionType = AcquisitionType::Adquisicion;
    terms.paymentType = PaymentType::Contado;
    terms.paymentMethod = method;
    terms.priceHundredths = price;
    return terms;
}

// Lo que la bitácora registra para una compra hecha con estos términos.
QMap<CashAccount, qint64> takenBy(const PurchaseTerms &terms)
{
    QMap<CashAccount, qint64> taken;
    for (const CashMovement &movement : CashFlowRules::forPurchase(terms, {}, kToday))
        taken[movement.account()] += movement.hundredths();
    return taken;
}

QStringList fieldsOf(const ValidationResult &result)
{
    return result.fields();
}

} // namespace

class TstCashFlow : public QObject
{
    Q_OBJECT

private slots:
    void accountsInScreenOrder()
    {
        QCOMPARE(allCashAccounts(),
                 (QList<CashAccount>{CashAccount::PettyCash, CashAccount::Bank, CashAccount::Safe}));
        QCOMPARE(displayLabel(CashAccount::Bank), QStringLiteral("Cantidad Banco"));
        QCOMPARE(cashAccountFromDb(toDbString(CashAccount::Safe)), std::optional<CashAccount>(CashAccount::Safe));
        QCOMPARE(accountFor(PaymentMethod::Efectivo), CashAccount::PettyCash);
        QCOMPARE(accountFor(PaymentMethod::Transferencia), CashAccount::Bank);
    }

    void movementKeepsItsSign()
    {
        const auto out = CashMovement::outflow(CashAccount::Bank, 500, CashMovementKind::Purchase, {}, kToday);
        QCOMPARE(out.hundredths(), qint64(-500));
        QVERIFY(!out.isInflow());
        QCOMPARE(CashMovement::inflow(CashAccount::Bank, -500, CashMovementKind::Sale, {}, kToday).hundredths(),
                 qint64(500));
        QCOMPARE(toHundredths(117.31), qint64(11731));
        QCOMPARE(cashMovementKindFromDb(QStringLiteral("Corrección de compra")),
                 std::optional<CashMovementKind>(CashMovementKind::PurchaseCorrection));
    }

    // Solo la compra de contado saca dinero, de la cuenta de su método.
    void onlyCashPurchasesMoveMoney()
    {
        const auto cash = CashFlowRules::forPurchase(cashPurchase(10000000), QStringLiteral("c"), kToday);
        QCOMPARE(cash.size(), 1);
        QCOMPARE(cash.first().account(), CashAccount::PettyCash);
        QCOMPARE(cash.first().hundredths(), qint64(-10000000));
        QCOMPARE(cash.first().kind(), CashMovementKind::Purchase);

        PurchaseTerms credit = cashPurchase(10000000);
        credit.paymentType = PaymentType::Credito;
        QVERIFY(CashFlowRules::forPurchase(credit, {}, kToday).isEmpty());

        PurchaseTerms consignment = cashPurchase(10000000);
        consignment.acquisitionType = AcquisitionType::Consignacion;
        QVERIFY(CashFlowRules::forPurchase(consignment, {}, kToday).isEmpty());
    }

    void unchangedCorrectionMovesNothing()
    {
        QVERIFY(CashFlowRules::forPurchaseCorrection(takenBy(cashPurchase(100)), cashPurchase(100), {}, kToday).isEmpty());
    }

    // Bajar el precio en la misma cuenta regresa solo la diferencia.
    void lowerPriceReturnsTheDifference()
    {
        const auto movements = CashFlowRules::forPurchaseCorrection(
            takenBy(cashPurchase(10000000)), cashPurchase(9000000), {}, kToday);
        QCOMPARE(movements.size(), 1);
        QCOMPARE(movements.first().account(), CashAccount::PettyCash);
        QCOMPARE(movements.first().hundredths(), qint64(1000000));
        QCOMPARE(movements.first().kind(), CashMovementKind::PurchaseCorrection);
    }

    // Cambiar de efectivo a transferencia regresa a Caja Chica y saca de Banco.
    void changingTheMethodMovesBetweenAccounts()
    {
        const auto movements = CashFlowRules::forPurchaseCorrection(
            takenBy(cashPurchase(500000, PaymentMethod::Efectivo)),
            cashPurchase(500000, PaymentMethod::Transferencia), {}, kToday);
        QCOMPARE(movements.size(), 2);
        QCOMPARE(movements.at(0).account(), CashAccount::PettyCash);
        QCOMPARE(movements.at(0).hundredths(), qint64(500000));
        QCOMPARE(movements.at(1).account(), CashAccount::Bank);
        QCOMPARE(movements.at(1).hundredths(), qint64(-500000));
    }

    // Se ajusta contra lo que de verdad registra la bitácora, no contra los
    // términos guardados: lo que nunca salió no se reembolsa.
    void correctionStartsFromWhatTheLedgerTook()
    {
        PurchaseTerms credit = cashPurchase(300000);
        credit.paymentType = PaymentType::Credito;
        QVERIFY(CashFlowRules::forPurchaseCorrection({}, credit, {}, kToday).isEmpty());
        const auto partial =
            CashFlowRules::forPurchaseCorrection({{CashAccount::PettyCash, -100000}}, cashPurchase(300000), {}, kToday);
        QCOMPARE(partial.size(), 1);
        QCOMPARE(partial.first().hundredths(), qint64(-200000));
    }

    // Pasar a crédito regresa lo que había salido; pasar a contado lo saca.
    void changingThePaymentType()
    {
        PurchaseTerms credit = cashPurchase(300000);
        credit.paymentType = PaymentType::Credito;
        const auto toCredit = CashFlowRules::forPurchaseCorrection(takenBy(cashPurchase(300000)), credit, {}, kToday);
        QCOMPARE(toCredit.size(), 1);
        QCOMPARE(toCredit.first().hundredths(), qint64(300000));
        const auto toCash = CashFlowRules::forPurchaseCorrection(takenBy(credit), cashPurchase(300000), {}, kToday);
        QCOMPARE(toCash.size(), 1);
        QCOMPARE(toCash.first().hundredths(), qint64(-300000));
    }

    void salesBringMoneyIn()
    {
        const auto cash = CashFlowRules::forCashSale(PaymentMethod::Transferencia, 25000000, {}, kToday);
        QCOMPARE(cash.size(), 1);
        QCOMPARE(cash.first().account(), CashAccount::Bank);
        QCOMPARE(cash.first().hundredths(), qint64(25000000));
        QCOMPARE(cash.first().kind(), CashMovementKind::Sale);

        const auto credit = CashFlowRules::forCreditSale(PaymentMethod::Efectivo, 5000000, {}, kToday);
        QCOMPARE(credit.size(), 1);
        QCOMPARE(credit.first().account(), CashAccount::PettyCash);
        QCOMPARE(credit.first().kind(), CashMovementKind::DownPayment);
        QVERIFY(CashFlowRules::forCreditSale(PaymentMethod::Efectivo, 0, {}, kToday).isEmpty());
    }

    void maintenanceAndExpensesTakeMoneyOut()
    {
        const auto maintenance = CashFlowRules::forMaintenance(PaymentMethod::Efectivo, 150000, {}, kToday);
        QCOMPARE(maintenance.first().hundredths(), qint64(-150000));
        QCOMPARE(maintenance.first().kind(), CashMovementKind::Maintenance);
        const auto expense = CashFlowRules::forExpense(PaymentMethod::Transferencia, 99, {}, kToday);
        QCOMPARE(expense.first().account(), CashAccount::Bank);
        QCOMPARE(expense.first().kind(), CashMovementKind::Expense);
    }

    // Por cuenta, siempre igual; en la misma cuenta, primero lo que entra.
    void applyOrderIsByAccountThenInflowFirst()
    {
        const QList<CashMovement> ordered = CashFlowRules::inApplyOrder({
            CashMovement::outflow(CashAccount::Safe, 1, CashMovementKind::Withdrawal, {}, kToday),
            CashMovement::outflow(CashAccount::PettyCash, 2, CashMovementKind::Purchase, {}, kToday),
            CashMovement::inflow(CashAccount::Bank, 3, CashMovementKind::Sale, {}, kToday),
            CashMovement::inflow(CashAccount::PettyCash, 4, CashMovementKind::Sale, {}, kToday),
        });
        QCOMPARE(ordered.at(0).hundredths(), qint64(4));
        QCOMPARE(ordered.at(1).hundredths(), qint64(-2));
        QCOMPARE(ordered.at(2).account(), CashAccount::Bank);
        QCOMPARE(ordered.at(3).account(), CashAccount::Safe);
    }

    void openingIsZeroOrMoreAndNotForTheSafe()
    {
        ValidationResult ok;
        CashMovementPolicy::checkOpening(CashAccount::PettyCash, 0, ok);
        CashMovementPolicy::checkOpening(CashAccount::Bank, 10000000, ok);
        QVERIFY(ok.isValid());

        ValidationResult negative;
        CashMovementPolicy::checkOpening(CashAccount::Bank, -1, negative);
        QCOMPARE(fieldsOf(negative), QStringList{QStringLiteral("bankOpening")});

        ValidationResult safe;
        CashMovementPolicy::checkOpening(CashAccount::Safe, 100, safe);
        QVERIFY(!safe.isValid());
    }

    // No se ingresa más de lo que hay en el origen.
    void depositIsLimitedByTheSource()
    {
        ValidationResult ok;
        CashMovementPolicy::checkSafeDeposit(CashAccount::PettyCash, 150000, 150000, ok);
        QVERIFY(ok.isValid());

        ValidationResult tooMuch;
        CashMovementPolicy::checkSafeDeposit(CashAccount::Bank, 150001, 150000, tooMuch);
        QCOMPARE(fieldsOf(tooMuch), QStringList{QStringLiteral("amount")});
        QCOMPARE(tooMuch.firstMessage(), QStringLiteral("Cantidad Banco solo tiene $1,500.00 disponibles."));

        ValidationResult zero;
        CashMovementPolicy::checkSafeDeposit(CashAccount::Bank, 0, 150000, zero);
        QCOMPARE(fieldsOf(zero), QStringList{QStringLiteral("amount")});

        ValidationResult fromSafe;
        CashMovementPolicy::checkSafeDeposit(CashAccount::Safe, 100, 150000, fromSafe);
        QCOMPARE(fieldsOf(fromSafe), QStringList{QStringLiteral("source")});
    }

    // Retiro: cuándo (no futuro), concepto y cuánto (no más de lo que hay).
    void withdrawalNeedsWhenWhatAndHowMuch()
    {
        ValidationResult ok;
        CashMovementPolicy::checkSafeWithdrawal(kToday, kToday, QStringLiteral("Pago de nómina"), 500, 500, ok);
        QVERIFY(ok.isValid());

        ValidationResult bad;
        CashMovementPolicy::checkSafeWithdrawal(kToday.addDays(1), kToday, QStringLiteral("  "), 501, 500, bad);
        QCOMPARE(fieldsOf(bad), (QStringList{QStringLiteral("date"), QStringLiteral("concept"),
                                             QStringLiteral("amount")}));

        ValidationResult longConcept;
        CashMovementPolicy::checkSafeWithdrawal(kToday, kToday, QString(201, QLatin1Char('x')), 1, 500,
                                                longConcept);
        QCOMPARE(fieldsOf(longConcept), QStringList{QStringLiteral("concept")});
    }
};

QTEST_APPLESS_MAIN(TstCashFlow)
#include "tst_cashflow.moc"
