// Pruebas de la regla del tope de pago en efectivo (LFPIORPI, 3,210 UMA).
// Fijan las dos decisiones que no se deducen del código: la regla es sobre el
// MÉTODO de pago (efectivo) y no sobre el tipo (contado), y sin UMA
// configurada el efectivo se rechaza en vez de dejarse pasar.

#include "domain/common/rules/cashpaymentlimit.h"

#include <QtTest>

using domain::CashPaymentLimit;
using domain::PaymentMethod;

class TstCashPaymentLimit : public QObject
{
    Q_OBJECT

private:
    // UMA redonda para que el tope sea exacto: 3210 × 100 = 321,000.
    static constexpr double kUma = 100.0;
    static constexpr double kLimit = 321000.0;

private slots:
    void limitIsUmaTimesMultiple()
    {
        const CashPaymentLimit rule(kUma);
        QVERIFY(rule.isConfigured());
        QCOMPARE(rule.limit(), kLimit);
    }

    void cashBelowLimitIsAllowed()
    {
        QVERIFY(CashPaymentLimit(kUma).allows(PaymentMethod::Efectivo, kLimit - 0.01));
    }

    // "Igual o superior" está prohibido: alcanzar el tope ya no se permite.
    void cashAtLimitIsRejected()
    {
        QVERIFY(!CashPaymentLimit(kUma).allows(PaymentMethod::Efectivo, kLimit));
    }

    // Con la UMA real el producto no es exacto en double: este caso es el que
    // atrapa una comparación ingenua.
    void cashAtRealLimitIsRejected()
    {
        const CashPaymentLimit rule(117.31); // UMA 2026; tope 376,565.10
        QVERIFY(!rule.allows(PaymentMethod::Efectivo, 376565.10));
        QVERIFY(rule.allows(PaymentMethod::Efectivo, 376565.09));
    }

    void cashAboveLimitIsRejected()
    {
        QVERIFY(!CashPaymentLimit(kUma).allows(PaymentMethod::Efectivo, kLimit + 1.0));
    }

    // Una transferencia es legal a cualquier monto.
    void transferAboveLimitIsAllowed()
    {
        QVERIFY(CashPaymentLimit(kUma).allows(PaymentMethod::Transferencia, kLimit * 10.0));
    }

    void cashWithoutUmaIsRejected()
    {
        const CashPaymentLimit rule(0.0);
        QVERIFY(!rule.isConfigured());
        QCOMPARE(rule.limit(), 0.0);
        QVERIFY(!rule.allows(PaymentMethod::Efectivo, 1.0));
    }

    void transferWithoutUmaIsAllowed()
    {
        QVERIFY(CashPaymentLimit(0.0).allows(PaymentMethod::Transferencia, kLimit * 10.0));
    }

    void checkReportsOnPaymentMethodField()
    {
        domain::ValidationResult result;
        CashPaymentLimit(kUma).check(PaymentMethod::Efectivo, kLimit, result);
        QCOMPARE(result.fields(), QStringList{QStringLiteral("paymentMethod")});
        // El mensaje muestra el tope en formato de México.
        QVERIFY2(result.firstMessage().contains(QStringLiteral("321,000.00")),
                 qPrintable(result.firstMessage()));
    }

    void checkWithoutUmaExplainsWhy()
    {
        domain::ValidationResult result;
        CashPaymentLimit(0.0).check(PaymentMethod::Efectivo, 1.0, result);
        QCOMPARE(result.fields(), QStringList{QStringLiteral("paymentMethod")});
        QVERIFY(result.firstMessage().contains(QStringLiteral("UMA")));
    }

    void checkAddsNothingWhenAllowed()
    {
        domain::ValidationResult result;
        CashPaymentLimit(kUma).check(PaymentMethod::Transferencia, kLimit * 2.0, result);
        CashPaymentLimit(kUma).check(PaymentMethod::Efectivo, 1000.0, result);
        QVERIFY(result.isValid());
    }

    void checkUsesTheGivenField()
    {
        domain::ValidationResult result;
        CashPaymentLimit(kUma).check(PaymentMethod::Efectivo, kLimit, result,
                                     QStringLiteral("salePayment"));
        QCOMPARE(result.fields(), QStringList{QStringLiteral("salePayment")});
    }
};

QTEST_APPLESS_MAIN(TstCashPaymentLimit)
#include "tst_cashpaymentlimit.moc"
