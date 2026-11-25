// Las reglas de un gasto operativo: fecha, tipo, factura con número, un
// subtotal que cabe en NUMERIC(12, 2) con su IVA del 16 % y método de pago.

#include "domain/finance/model/operationalexpense.h"
#include "domain/finance/rules/financeaccess.h"
#include "domain/finance/value_objects/expenseamounts.h"
#include "domain/finance/value_objects/expensetrend.h"

#include <QtTest>

namespace {

domain::OperationalExpense withSubtotal(const QString &subtotal)
{
    return {QDate(2026, 11, 24), 1, false, QString(), subtotal, domain::PaymentMethod::Efectivo};
}

domain::OperationalExpense withInvoice(bool hasInvoice, const QString &number)
{
    return {QDate(2026, 11, 24), 1, hasInvoice, number, QStringLiteral("100"),
            domain::PaymentMethod::Transferencia};
}

} // namespace

class TstOperationalExpense : public QObject
{
    Q_OBJECT

private slots:
    void onlyTheAdminUsesFinance()
    {
        QVERIFY(domain::FinanceAccess(domain::UserRole::Admin).canUseFinance());
        QVERIFY(!domain::FinanceAccess(domain::UserRole::SalesAgent).canUseFinance());
    }

    void theTrendIsThePercentChangeAgainstThePreviousMonth()
    {
        using domain::ExpenseTrend;
        QCOMPARE(ExpenseTrend::percentChange(1200.0, 1000.0), std::optional<double>(20.0));
        QCOMPARE(ExpenseTrend::percentChange(750.0, 1000.0), std::optional<double>(-25.0));
        QCOMPARE(ExpenseTrend::percentChange(0.0, 0.0), std::optional<double>(0.0));
        // Sin gastos el mes anterior no hay base para un porcentaje.
        QCOMPARE(ExpenseTrend::percentChange(500.0, 0.0), std::nullopt);
    }

    void aCompleteExpenseIsValid()
    {
        const domain::OperationalExpense expense = withInvoice(true, QStringLiteral(" A-123 "));
        QVERIFY(expense.validate().isValid());
        QCOMPARE(expense.invoiceNumber(), QStringLiteral("A-123"));
    }

    void anEmptyExpenseReportsEachField()
    {
        const domain::OperationalExpense expense(QDate(), -1, true, QString(), QString(),
                                                 std::nullopt);
        QCOMPARE(expense.validate().fields(),
                 (QStringList{QStringLiteral("expenseDate"), QStringLiteral("expenseType"),
                              QStringLiteral("invoiceNumber"), QStringLiteral("subtotal"),
                              QStringLiteral("paymentMethod")}));
    }

    void withoutInvoiceTheNumberIsDropped()
    {
        const domain::OperationalExpense expense = withInvoice(false, QStringLiteral("A-123"));
        QVERIFY(expense.validate().isValid());
        QVERIFY(expense.invoiceNumber().isEmpty());
    }

    void anInvoiceNumberTooLongIsRejected()
    {
        const QString number(domain::OperationalExpense::kInvoiceNumberMaxLength + 1,
                             QLatin1Char('9'));
        QCOMPARE(domain::OperationalExpense::validateInvoice(true, number).fields(),
                 QStringList{QStringLiteral("invoiceNumber")});
    }

    void ivaIsSixteenPercentRoundedToTheCent_data()
    {
        QTest::addColumn<QString>("subtotal");
        QTest::addColumn<QString>("iva");
        QTest::addColumn<QString>("total");
        QTest::newRow("entero") << QStringLiteral("1000") << QStringLiteral("160.00")
                                << QStringLiteral("1160.00");
        QTest::newRow("un decimal") << QStringLiteral("1500.5") << QStringLiteral("240.08")
                                    << QStringLiteral("1740.58");
        // 0.0048: no llega a medio centavo.
        QTest::newRow("redondea abajo") << QStringLiteral("0.03") << QStringLiteral("0.00")
                                        << QStringLiteral("0.03");
        // 0.016: pasa de medio centavo.
        QTest::newRow("redondea arriba") << QStringLiteral("0.10") << QStringLiteral("0.02")
                                         << QStringLiteral("0.12");
        QTest::newRow("exacto") << QStringLiteral("0.25") << QStringLiteral("0.04")
                                << QStringLiteral("0.29");
        QTest::newRow("cero") << QStringLiteral("0") << QStringLiteral("0.00")
                              << QStringLiteral("0.00");
    }
    void ivaIsSixteenPercentRoundedToTheCent()
    {
        QFETCH(QString, subtotal);
        QFETCH(QString, iva);
        QFETCH(QString, total);
        const std::optional<domain::ExpenseAmounts> amounts =
            domain::ExpenseAmounts::fromSubtotal(subtotal);
        QVERIFY(amounts);
        QCOMPARE(amounts->iva(), iva);
        QCOMPARE(amounts->total(), total);
    }

    void subtotalsThatDoNotFit_data()
    {
        QTest::addColumn<QString>("subtotal");
        QTest::newRow("vacío") << QString();
        QTest::newRow("texto") << QStringLiteral("mil");
        QTest::newRow("negativo") << QStringLiteral("-10");
        QTest::newRow("tres decimales") << QStringLiteral("10.505");
        QTest::newRow("once enteros") << QStringLiteral("10000000000");
        QTest::newRow("coma de miles") << QStringLiteral("1,500");
        // Cabe como subtotal, pero con el IVA el total ya no.
        QTest::newRow("el total no cabe") << QStringLiteral("9000000000");
    }
    void subtotalsThatDoNotFit()
    {
        QFETCH(QString, subtotal);
        QCOMPARE(withSubtotal(subtotal).validate().fields(),
                 QStringList{QStringLiteral("subtotal")});
    }

    void theTotalMustFitTheColumn()
    {
        // Total 9,999,999,999.99: el mayor que cabe en NUMERIC(12, 2).
        QVERIFY(withSubtotal(QStringLiteral("8620689655.16")).validate().isValid());
        // Total 10,000,000,000.00: ya no cabe.
        QCOMPARE(withSubtotal(QStringLiteral("8620689655.17")).validate().fields(),
                 QStringList{QStringLiteral("subtotal")});
    }
};

QTEST_APPLESS_MAIN(TstOperationalExpense)
#include "tst_operationalexpense.moc"
