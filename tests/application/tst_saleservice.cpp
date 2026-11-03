#include "application/commercial/sales/services/saleservice.h"

#include <QtTest>

namespace {

class FakeSaleRepository final : public application::SaleRepository
{
public:
    QList<application::SaleVehicleOptionDto> searchAvailableVehicles(const QString &text,
                                                                       QString *error) override
    {
        lastVehicleQuery = text;
        if (error)
            error->clear();
        return vehicleResults;
    }

    QList<application::SaleVendorOptionDto> searchVendors(const QString &text,
                                                           QString *error) override
    {
        ++vendorSearchCalls;
        lastVendorQuery = text;
        if (error)
            error->clear();
        return vendorResults;
    }

    application::SaleRegistrationResultDto registerSale(
        const application::SaleRegistrationDto &sale) override
    {
        ++registerCalls;
        lastSale = sale;
        application::SaleRegistrationResultDto result;
        result.status = application::SaleRegistrationResultDto::Status::Registered;
        result.saleFolio = 91;
        return result;
    }

    QString lastVehicleQuery;
    QString lastVendorQuery;
    QList<application::SaleVehicleOptionDto> vehicleResults;
    QList<application::SaleVendorOptionDto> vendorResults;
    application::SaleRegistrationDto lastSale;
    int registerCalls = 0;
    int vendorSearchCalls = 0;
};

class FakeCounterpartyDirectory final : public application::CounterpartyDirectory
{
public:
    QList<application::CounterpartyDto> search(const QString &query, QString *error) override
    {
        lastQuery = query;
        if (error)
            error->clear();
        return results;
    }

    application::CounterpartyUpdateResultDto update(
        const application::CounterpartyDto &counterparty) override
    {
        ++updateCalls;
        lastUpdated = counterparty;
        return {updateSucceeds, {}, updateSucceeds ? QString() : QStringLiteral("update failed")};
    }

    QString lastQuery;
    QList<application::CounterpartyDto> results;
    application::CounterpartyDto lastUpdated;
    int updateCalls = 0;
    bool updateSucceeds = true;
};

application::SaleRegistrationDto validCashSale()
{
    application::SaleRegistrationDto sale;
    sale.vehicleFolio = 12;
    sale.vendorId = 3;
    sale.saleDate = QDate(2026, 10, 22);
    sale.buyer.fullName = QStringLiteral("Comprador Ejemplo");
    sale.buyer.streetAddress = QStringLiteral("Av. Ejemplo 123");
    sale.buyer.suburb = QStringLiteral("Centro");
    sale.buyer.locality = QStringLiteral("Monterrey");
    sale.buyer.state = QStringLiteral("Nuevo León");
    sale.buyer.postalCode = QStringLiteral("64000");
    sale.buyer.phone = QStringLiteral("8180000000");
    sale.buyer.email = QStringLiteral("comprador@example.com");
    sale.paymentType = domain::PaymentType::Contado;
    sale.paymentMethod = domain::PaymentMethod::Transferencia;
    sale.subtotal = 100000.0;
    sale.iva = 16000.0;
    sale.totalAmount = 116000.0;
    sale.observations = QStringLiteral("Venta de prueba");
    return sale;
}

} // namespace

class TstSaleService : public QObject
{
    Q_OBJECT

private slots:
    void searchWaitsForThreeCharacters()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);

        QVERIFY(service.searchAvailableVehicles(QStringLiteral("  ab ")).vehicles.isEmpty());
        QVERIFY(repository.lastVehicleQuery.isEmpty());
        QVERIFY(service.searchVendors(QStringLiteral("xy")).vendors.isEmpty());
        QVERIFY(repository.lastVendorQuery.isEmpty());

        (void)service.searchAvailableVehicles(QStringLiteral("  civic "));
        QCOMPARE(repository.lastVehicleQuery, QStringLiteral("civic"));
        (void)service.searchVendors(QStringLiteral("  Ana "));
        QCOMPARE(repository.lastVendorQuery, QStringLiteral("Ana"));

        QVERIFY(service.searchCounterparties(QStringLiteral("id")).counterparties.isEmpty());
        QVERIFY(counterparties.lastQuery.isEmpty());
        (void)service.searchCounterparties(QStringLiteral("  Ana "));
        QCOMPARE(counterparties.lastQuery, QStringLiteral("Ana"));
    }

    void vehicleSearchAllowsEmptyQueryForDefaultList()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        repository.vehicleResults << application::SaleVehicleOptionDto{12, QStringLiteral("Toyota"), QStringLiteral("Corolla"), 2022, 320000.0};
        repository.vehicleResults << application::SaleVehicleOptionDto{13, QStringLiteral("Honda"), QStringLiteral("Civic"), 2023, 345000.0};

        const auto result = service.searchAvailableVehicles(QString());

        QCOMPARE(result.vehicles.size(), 2);
        QCOMPARE(repository.lastVehicleQuery, QString());
    }

    void vendorSearchAllowsEmptyQueryForRecentUsers()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);

        (void)service.searchVendors(QString());

        QCOMPARE(repository.vendorSearchCalls, 1);
        QVERIFY(repository.lastVendorQuery.isEmpty());
    }

    void validCashSaleIsDelegatedWithoutChangingInputs()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        const application::SaleRegistrationDto sale = validCashSale();

        const auto result = service.registerSale(sale);

        QCOMPARE(result.status, application::SaleRegistrationResultDto::Status::Registered);
        QCOMPARE(result.saleFolio, 91);
        QCOMPARE(repository.registerCalls, 1);
        QCOMPARE(repository.lastSale.subtotal, sale.subtotal);
        QCOMPARE(repository.lastSale.iva, sale.iva);
    }

    void invalidSaleIsRejectedBeforePersistence()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        application::SaleRegistrationDto sale = validCashSale();
        sale.vehicleFolio = -1;

        const auto result = service.registerSale(sale);

        QCOMPARE(result.status, application::SaleRegistrationResultDto::Status::Rejected);
        QCOMPARE(result.field, QStringLiteral("vehicle"));
        QCOMPARE(repository.registerCalls, 0);
    }

    void duplicateNationalIdForNewBuyerIsRejectedBeforeSaving()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        counterparties.results << application::CounterpartyDto{7,
                                                              QStringLiteral("Ana López"),
                                                              QStringLiteral("ABC123456"),
                                                              QStringLiteral("Av. Central 123"),
                                                              QStringLiteral("Centro"),
                                                              QStringLiteral("Monterrey"),
                                                              QStringLiteral("Nuevo León"),
                                                              QStringLiteral("64000"),
                                                              QStringLiteral("8180000000"),
                                                              QStringLiteral("ana@example.com")};
        application::SaleService service(repository, counterparties);
        application::SaleRegistrationDto sale = validCashSale();
        sale.buyer.nationalId = QStringLiteral("ABC123456");

        const auto result = service.registerSale(sale);

        QCOMPARE(result.status, application::SaleRegistrationResultDto::Status::Rejected);
        QCOMPARE(result.field, QStringLiteral("buyer.nationalId"));
        QCOMPARE(result.message, QStringLiteral("Ya existe una persona registrada con ese número de identificación."));
        QCOMPARE(repository.registerCalls, 0);
    }

    void requiredBuyerAndSaleFieldsAreReportedTogether()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        application::SaleRegistrationDto sale = validCashSale();
        sale.buyer.fullName.clear();
        sale.buyer.locality.clear();
        sale.buyer.phone.clear();
        sale.observations.clear();

        const domain::ValidationResult result = service.validateSale(sale);

        QVERIFY(!result.isValid());
        QVERIFY(result.fields().contains(QStringLiteral("buyer.fullName")));
        QVERIFY(result.fields().contains(QStringLiteral("buyer.locality")));
        QVERIFY(result.fields().contains(QStringLiteral("buyer.phone")));
        QVERIFY(result.fields().contains(QStringLiteral("observations")));
        QVERIFY(!result.fields().contains(QStringLiteral("buyer.nationalId")));
    }

    void creditRequiresCompleteGuarantorDetails()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        application::SaleRegistrationDto sale = validCashSale();
        sale.paymentType = domain::PaymentType::Credito;
        sale.paymentMethod.reset();
        application::SaleFinancingDto financing;
        sale.financing = financing;

        const domain::ValidationResult result = service.validateSale(sale);

        QVERIFY(result.fields().contains(QStringLiteral("guarantor.fullName")));
        QVERIFY(result.fields().contains(QStringLiteral("guarantor.nationalId")));
        QVERIFY(result.fields().contains(QStringLiteral("guarantor.postalCode")));
    }

    void creditRequiresGuarantorAndFinancingData()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        application::SaleRegistrationDto sale = validCashSale();
        sale.paymentType = domain::PaymentType::Credito;
        sale.paymentMethod.reset();

        const auto result = service.registerSale(sale);

        QCOMPARE(result.status, application::SaleRegistrationResultDto::Status::Rejected);
        QCOMPARE(result.field, QStringLiteral("guarantor.fullName"));
        QCOMPARE(repository.registerCalls, 0);
    }

    void counterpartyUpdatesRequireAnExistingSelection()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        application::CounterpartyDto person;
        person.fullName = QStringLiteral("Ana López");
        person.nationalId = QStringLiteral("INE-123");

        const auto result = service.updateCounterparty(person);

        QVERIFY(!result.ok);
        QCOMPARE(result.field, QStringLiteral("counterparty"));
        QCOMPARE(counterparties.updateCalls, 0);
    }

    void counterpartyUpdatesAreExplicitAndPreserveSelectedId()
    {
        FakeSaleRepository repository;
        FakeCounterpartyDirectory counterparties;
        application::SaleService service(repository, counterparties);
        application::CounterpartyDto person;
        person.id = 42;
        person.fullName = QStringLiteral("Ana López Actualizada");
        person.nationalId = QStringLiteral("INE-123");

        const auto result = service.updateCounterparty(person);

        QVERIFY(result.ok);
        QCOMPARE(counterparties.updateCalls, 1);
        QCOMPARE(counterparties.lastUpdated.id, 42);
        QCOMPARE(counterparties.lastUpdated.fullName, person.fullName);
    }
};

QTEST_APPLESS_MAIN(TstSaleService)
#include "tst_saleservice.moc"
