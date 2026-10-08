// Pruebas de los mecanismos de POO del dominio: el polimorfismo de las dos
// ramas de Vehicle, el Template Method de validate(), el Visitor y el
// VehicleBuilder como único lugar que elige la subclase. Fijan el
// comportamiento que la rúbrica evalúa, no solo que compile.

#include "domain/acquiredvehicle.h"
#include "domain/consignedvehicle.h"
#include "domain/vehiclevisitor.h"
#include "vehiclefixtures.h"

#include <QtTest>

namespace {

// Visitor de prueba: registra qué sobrecarga eligió el doble despacho.
class RecordingVisitor final : public domain::VehicleVisitor
{
public:
    void visit(const domain::AcquiredVehicle &) override { visited = QStringLiteral("acquired"); }
    void visit(const domain::ConsignedVehicle &) override { visited = QStringLiteral("consigned"); }

    QString visited;
};

std::unique_ptr<domain::Vehicle> buildOrFail(domain::VehicleBuilder &builder)
{
    builder.setConditions(fixtures::validConditions());
    domain::ValidationResult result;
    std::unique_ptr<domain::Vehicle> vehicle = builder.build(result);
    if (!vehicle)
        qWarning("build() falló: %s", qPrintable(result.joinedMessages()));
    return vehicle;
}

} // namespace

class TstVehiclePolymorphism : public QObject
{
    Q_OBJECT

private slots:
    // --- El builder elige la subclase -------------------------------------
    void builderBuildsTheBranchSubclass()
    {
        auto acquisition = fixtures::validAcquisition();
        auto consignment = fixtures::validConsignment();
        const auto acquired = buildOrFail(acquisition);
        const auto consigned = buildOrFail(consignment);
        QVERIFY(acquired);
        QVERIFY(consigned);
        QCOMPARE(acquired->acquisitionType(), domain::AcquisitionType::Adquisicion);
        QCOMPARE(consigned->acquisitionType(), domain::AcquisitionType::Consignacion);
    }

    void buildFailsWithErrorsAndNoObject()
    {
        domain::VehicleBuilder builder;
        domain::ValidationResult result;
        QVERIFY(!builder.build(result));
        QVERIFY(!result.isValid());
    }

    // Tras entregar la unidad el builder queda vacío: no comparte el objeto.
    void builderIsEmptyAfterBuild()
    {
        auto builder = fixtures::validAcquisition();
        QVERIFY(buildOrFail(builder));
        domain::ValidationResult second;
        QVERIFY(!builder.build(second));
    }

    // Cambiar de rama reinicia la captura: un precio de compra no es un
    // precio base.
    void switchingBranchResetsTheCapture()
    {
        auto builder = fixtures::validAcquisition();
        builder.setAcquisitionType(domain::AcquisitionType::Consignacion);
        const QStringList fields = builder.validateVehicleData().fields();
        QVERIFY(fields.contains(QStringLiteral("basePrice")));
        QVERIFY(fields.contains(QStringLiteral("serialNumber")));
    }

    // --- Polimorfismo -----------------------------------------------------
    void salePriceDependsOnTheBranch()
    {
        auto acquisition = fixtures::validAcquisition();
        auto consignment = fixtures::validConsignment();
        const auto acquired = buildOrFail(acquisition);
        const auto consigned = buildOrFail(consignment);
        QVERIFY(acquired && consigned);
        // Compra: el precio que se capturó.
        QCOMPARE(acquired->salePrice(), 180000.0);
        // Consignación: base + comisión (200,000 + 10 %).
        QCOMPARE(consigned->salePrice(), 220000.0);
    }

    void expectedProfitDependsOnTheBranch()
    {
        auto acquisition = fixtures::validAcquisition();
        auto consignment = fixtures::validConsignment();
        const auto acquired = buildOrFail(acquisition);
        const auto consigned = buildOrFail(consignment);
        QVERIFY(acquired && consigned);
        // Compra: venta - compra.
        QCOMPARE(acquired->expectedProfit(), 30000.0);
        // Consignación: solo la comisión; la unidad no es de la agencia.
        QCOMPARE(consigned->expectedProfit(), 20000.0);
    }

    void invoiceTypesAreDisjointPerBranch()
    {
        using domain::InvoiceType;
        const domain::AcquiredVehicle acquired;
        const domain::ConsignedVehicle consigned;
        // Los cuatro valores, en las dos ramas: cada uno lo acepta exactamente
        // una de ellas.
        for (InvoiceType type : {InvoiceType::Facturado, InvoiceType::Autofactura,
                                 InvoiceType::FacturadoReal, InvoiceType::NoFacturado}) {
            const bool forAcquisition =
                type == InvoiceType::Facturado || type == InvoiceType::Autofactura;
            QCOMPARE(acquired.acceptsInvoiceType(type), forAcquisition);
            QCOMPARE(consigned.acceptsInvoiceType(type), !forAcquisition);
        }
    }

    // --- Template Method ----------------------------------------------------
    // validate() hace las reglas comunes y luego delega lo propio de cada rama
    // en collectSpecificErrors().
    void validateCombinesCommonAndBranchRules()
    {
        const QStringList acquired = domain::AcquiredVehicle().validate().fields();
        const QStringList consigned = domain::ConsignedVehicle().validate().fields();

        // Comunes a las dos ramas.
        QVERIFY(acquired.contains(QStringLiteral("serialNumber")));
        QVERIFY(consigned.contains(QStringLiteral("serialNumber")));

        // Propias de cada una.
        QVERIFY(acquired.contains(QStringLiteral("purchasePrice")));
        QVERIFY(!acquired.contains(QStringLiteral("basePrice")));
        QVERIFY(consigned.contains(QStringLiteral("basePrice")));
        QVERIFY(!consigned.contains(QStringLiteral("purchasePrice")));
    }

    // --- Visitor ------------------------------------------------------------
    void visitorDispatchesOnTheDynamicType()
    {
        auto acquisition = fixtures::validAcquisition();
        auto consignment = fixtures::validConsignment();
        const std::unique_ptr<domain::Vehicle> acquired = buildOrFail(acquisition);
        const std::unique_ptr<domain::Vehicle> consigned = buildOrFail(consignment);
        QVERIFY(acquired && consigned);

        RecordingVisitor visitor;
        acquired->accept(visitor);
        QCOMPARE(visitor.visited, QStringLiteral("acquired"));
        consigned->accept(visitor);
        QCOMPARE(visitor.visited, QStringLiteral("consigned"));
    }

    // --- Validación por paso --------------------------------------------------
    // Cada paso valida lo suyo: el Paso 2 no reclama datos del Paso 1.
    void conditionValidationIgnoresVehicleData()
    {
        domain::VehicleBuilder builder;
        builder.setConditions(fixtures::validConditions());
        QVERIFY(builder.validateConditionData().isValid());
        QVERIFY(!builder.validateVehicleData().isValid());
    }
};

QTEST_APPLESS_MAIN(TstVehiclePolymorphism)
#include "tst_vehiclepolymorphism.moc"
