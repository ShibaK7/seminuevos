// Pruebas de los campos obligatorios. La regla del negocio es "mandan los
// asteriscos": todo campo que la pantalla marca como requerido lo exige el
// dominio, salvo Folio (lo asigna la base) y Fecha (siempre tiene valor).
// Estas pruebas fijan esa lista para las dos ramas y para el Paso 2.

#include "vehiclefixtures.h"

#include <QtTest>

class TstVehicle : public QObject
{
    Q_OBJECT

private:
    static void expectMissing(const domain::VehicleBuilder &builder, const QString &field)
    {
        const QStringList fields = builder.validateVehicleData().fields();
        QVERIFY2(fields.contains(field),
                 qPrintable(QStringLiteral("se esperaba error en %1; errores: %2")
                                .arg(field, fields.join(QStringLiteral(", ")))));
    }

private slots:
    // --- Paso 1, campos comunes ------------------------------------------
    void validConsignmentPasses()
    {
        const auto builder = fixtures::validConsignment();
        const domain::ValidationResult result = builder.validateVehicleData();
        QVERIFY2(result.isValid(), qPrintable(result.joinedMessages()));
    }

    void subtypeIsRequired()
    {
        auto builder = fixtures::validAcquisition();
        builder.setSubtype(domain::CatalogRef{});
        expectMissing(builder, QStringLiteral("subtype"));
    }

    void motorNumberIsRequired()
    {
        auto builder = fixtures::validAcquisition();
        builder.setMotorNumber(QString());
        expectMissing(builder, QStringLiteral("motorNumber"));
    }

    void repuveIsRequired()
    {
        auto builder = fixtures::validAcquisition();
        builder.setRepuve(QString());
        expectMissing(builder, QStringLiteral("repuve"));
    }

    void platesAreRequired()
    {
        auto builder = fixtures::validAcquisition();
        builder.setPlates(QString());
        expectMissing(builder, QStringLiteral("plates"));
    }

    void platesHolderIsRequired()
    {
        auto builder = fixtures::validAcquisition();
        builder.setPlatesHolder(QString());
        expectMissing(builder, QStringLiteral("platesHolder"));
    }

    void invoiceNumberIsRequiredWhenInvoiced()
    {
        auto builder = fixtures::validAcquisition();
        builder.setInvoiceNumber(QString());
        expectMissing(builder, QStringLiteral("invoiceNumber"));

        auto consignment = fixtures::validConsignment(); // Facturado REAL
        consignment.setInvoiceNumber(QString());
        expectMissing(consignment, QStringLiteral("invoiceNumber"));
    }

    // Autofactura sigue exigiendo el número (solo "No Facturado" se libra).
    void invoiceNumberIsRequiredForSelfInvoice()
    {
        auto builder = fixtures::validAcquisition();
        builder.setInvoiceType(domain::InvoiceType::Autofactura).setInvoiceNumber(QString());
        expectMissing(builder, QStringLiteral("invoiceNumber"));
    }

    // Una unidad "No Facturada" no tiene factura: no se le puede exigir número.
    void invoiceNumberIsOptionalWhenNotInvoiced()
    {
        auto builder = fixtures::validConsignment();
        builder.setInvoiceType(domain::InvoiceType::NoFacturado).setInvoiceNumber(QString());
        const domain::ValidationResult result = builder.validateVehicleData();
        QVERIFY2(result.isValid(), qPrintable(result.joinedMessages()));
    }

    void counterpartyIdIsRequired()
    {
        domain::Counterparty owner;
        (void)owner.setFullName(QStringLiteral("Juan Pérez López"));
        auto builder = fixtures::validAcquisition();
        builder.setCounterparty(owner);
        expectMissing(builder, QStringLiteral("counterparty.nationalId"));
    }

    void emptyAcquisitionReportsEveryMarkedField()
    {
        domain::VehicleBuilder builder;
        builder.setAcquisitionType(domain::AcquisitionType::Adquisicion);
        const QStringList fields = builder.validateVehicleData().fields();
        const QStringList expected = {
            QStringLiteral("serialNumber"), QStringLiteral("model"),
            QStringLiteral("yearModel"), QStringLiteral("brand"), QStringLiteral("vehicleType"),
            QStringLiteral("subtype"), QStringLiteral("motorNumber"),
            QStringLiteral("repuve"), QStringLiteral("plates"),
            QStringLiteral("platesHolder"), QStringLiteral("invoiceNumber"),
            QStringLiteral("counterparty.fullName"), QStringLiteral("counterparty.nationalId"),
            QStringLiteral("purchasePrice"), QStringLiteral("salePrice"),
        };
        for (const QString &field : expected)
            QVERIFY2(fields.contains(field), qPrintable(field));
    }

    // --- Paso 1, solo consignación ---------------------------------------
    void consignmentRequiresBasePriceButNoPurchaseData()
    {
        domain::VehicleBuilder builder;
        builder.setAcquisitionType(domain::AcquisitionType::Consignacion);
        fixtures::fillCommonData(builder);
        builder.setInvoiceType(domain::InvoiceType::FacturadoReal);
        const QStringList fields = builder.validateVehicleData().fields();
        QVERIFY(fields.contains(QStringLiteral("basePrice")));
        QVERIFY(!fields.contains(QStringLiteral("purchasePrice")));
        QVERIFY(!fields.contains(QStringLiteral("salePrice")));
        QVERIFY(!fields.contains(QStringLiteral("paymentMethod")));
    }

    // --- Paso 2 -----------------------------------------------------------
    void completeConditionsPass()
    {
        auto builder = fixtures::validAcquisition();
        builder.setConditions(fixtures::validConditions());
        const domain::ValidationResult result = builder.validateConditionData();
        QVERIFY2(result.isValid(), qPrintable(result.joinedMessages()));
    }

    // Sin elegir nada, cada combo marcado reporta su faltante: ya no se
    // guardan valores por omisión que nadie capturó.
    void emptyConditionsReportEveryMarkedField()
    {
        auto builder = fixtures::validAcquisition();
        builder.setConditions(domain::VehicleConditions{});
        const QStringList fields = builder.validateConditionData().fields();
        const QStringList expected = {
            QStringLiteral("conditions.fuelType"), QStringLiteral("conditions.cylinders"),
            QStringLiteral("conditions.transmission"), QStringLiteral("conditions.interiorMaterial"),
            QStringLiteral("conditions.windowRegulators"),
            QStringLiteral("conditions.airConditioning"),
        };
        for (const QString &field : expected)
            QVERIFY2(fields.contains(field), qPrintable(field));
    }

    void interiorMaterialIsRequired()
    {
        domain::VehicleConditions conditions = fixtures::validConditions();
        (void)conditions.setInteriorMaterial(QString());
        auto builder = fixtures::validAcquisition();
        builder.setConditions(conditions);
        QVERIFY(builder.validateConditionData().fields().contains(
            QStringLiteral("conditions.interiorMaterial")));
    }

    // Los pasos no se reclaman datos entre sí: el Paso 1 no exige condiciones.
    void vehicleDataIgnoresConditions()
    {
        auto builder = fixtures::validAcquisition();
        builder.setConditions(domain::VehicleConditions{});
        QVERIFY(builder.validateVehicleData().isValid());
    }
};

QTEST_APPLESS_MAIN(TstVehicle)
#include "tst_vehicle.moc"
