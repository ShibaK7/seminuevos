// Pruebas de las reglas propias de una compra: precios y el tope de efectivo
// aplicado por AcquiredVehicle. La más importante es la regresión del criterio
// del tope: antes se evaluaba sobre el TIPO de pago (contado) y ahora sobre el
// MÉTODO (efectivo).

#include "domain/model/acquiredvehicle.h"
#include "vehiclefixtures.h"

#include <QtTest>

using domain::PaymentMethod;
using domain::PaymentType;

class TstAcquiredVehicle : public QObject
{
    Q_OBJECT

private:
    static QStringList errorFields(const domain::VehicleBuilder &builder)
    {
        return builder.validateVehicleData().fields();
    }

private slots:
    void validAcquisitionPasses()
    {
        const auto builder = fixtures::validAcquisition();
        const domain::ValidationResult result = builder.validateVehicleData();
        QVERIFY2(result.isValid(), qPrintable(result.joinedMessages()));
    }

    void salePriceIsRequired()
    {
        auto builder = fixtures::validAcquisition();
        builder.setSalePrice(0.0);
        QVERIFY(errorFields(builder).contains(QStringLiteral("salePrice")));
    }

    void purchasePriceIsRequired()
    {
        domain::VehicleBuilder builder;
        builder.setAcquisitionType(domain::AcquisitionType::Adquisicion);
        fixtures::fillCommonData(builder);
        builder.setInvoiceType(domain::InvoiceType::Facturado).setSalePrice(180000.0);
        QVERIFY(errorFields(builder).contains(QStringLiteral("purchasePrice")));
    }

    void cashBelowLimitPasses()
    {
        auto builder = fixtures::validAcquisition();
        builder.setPaymentMethod(PaymentMethod::Efectivo).setPurchasePrice(300000.0);
        QVERIFY(!errorFields(builder).contains(QStringLiteral("paymentMethod")));
    }

    void cashAboveLimitIsRejected()
    {
        auto builder = fixtures::validAcquisition();
        builder.setPaymentMethod(PaymentMethod::Efectivo).setPurchasePrice(400000.0);
        QVERIFY(errorFields(builder).contains(QStringLiteral("paymentMethod")));
    }

    // Regresión: con la regla vieja, un contado por transferencia arriba del
    // tope se rechazaba. Es legal.
    void cashOnTransferAboveLimitPasses()
    {
        auto builder = fixtures::validAcquisition();
        builder.setPaymentType(PaymentType::Contado)
            .setPaymentMethod(PaymentMethod::Transferencia)
            .setPurchasePrice(900000.0);
        const QStringList fields = errorFields(builder);
        QVERIFY(!fields.contains(QStringLiteral("paymentMethod")));
        QVERIFY(!fields.contains(QStringLiteral("paymentType")));
    }

    // Regresión: con la regla vieja, un crédito liquidado en efectivo arriba
    // del tope pasaba. Está prohibido.
    void creditPaidInCashAboveLimitIsRejected()
    {
        auto builder = fixtures::validAcquisition();
        builder.setPaymentType(PaymentType::Credito)
            .setPaymentMethod(PaymentMethod::Efectivo)
            .setPurchasePrice(400000.0);
        QVERIFY(errorFields(builder).contains(QStringLiteral("paymentMethod")));
    }

    void cashWithoutUmaIsRejected()
    {
        auto builder = fixtures::validAcquisition(0.0);
        builder.setPaymentMethod(PaymentMethod::Efectivo).setPurchasePrice(1000.0);
        QVERIFY(errorFields(builder).contains(QStringLiteral("paymentMethod")));
    }

    void transferWithoutUmaPasses()
    {
        const auto builder = fixtures::validAcquisition(0.0);
        const domain::ValidationResult result = builder.validateVehicleData();
        QVERIFY2(result.isValid(), qPrintable(result.joinedMessages()));
    }
};

QTEST_APPLESS_MAIN(TstAcquiredVehicle)
#include "tst_acquiredvehicle.moc"
