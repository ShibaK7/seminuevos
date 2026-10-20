// Caso de uso del historial de mantenimientos (US-03.3) con puertos falsos:
// qué lee el formulario de alta, el historial de un folio, el alta (#46) y a
// quién se le niega todo eso.

#include "application/inventory/maintenance/services/maintenanceservice.h"
#include "fakes.h"

#include <QtTest>

using LookupStatus = application::MaintenanceFormLookupsResult::Status;
using HistoryStatus = application::MaintenanceHistoryResult::Status;
using AddStatus = application::MaintenanceAddResult::Status;
using domain::UserRole;

namespace {

application::NewMaintenanceDto captured(const QString &cost = QStringLiteral("1500.50"))
{
    application::NewMaintenanceDto dto;
    dto.vehicleFolio = 7;
    dto.date = QDate(2026, 10, 19);
    dto.maintenanceTypeId = 1;
    dto.cost = cost;
    dto.description = QStringLiteral("Cambio de aceite");
    return dto;
}

} // namespace

class TstMaintenanceService : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryMaintenanceRepository m_maintenances;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeMaintenanceReceiptGenerator m_receipts;

    application::MaintenanceService service() { return {m_maintenances, m_reference, m_receipts}; }

private slots:
    void init()
    {
        m_maintenances = fakes::InMemoryMaintenanceRepository();
        m_reference = fakes::FakeReferenceDataReader();
        m_receipts = fakes::FakeMaintenanceReceiptGenerator();
    }

    // --- Catálogo del formulario ---

    void adminGetsTheMaintenanceTypes()
    {
        const application::MaintenanceFormLookupsResult result =
            service().formLookups(UserRole::Admin);

        QCOMPARE(result.status, LookupStatus::Loaded);
        QCOMPARE(result.maintenanceTypes.size(), 2);
        QCOMPARE(result.maintenanceTypes.first().name, QStringLiteral("Mecánica"));
    }

    void sellerIsDeniedEvenIfTheSectionWereShown()
    {
        const application::MaintenanceFormLookupsResult result =
            service().formLookups(UserRole::SalesAgent);

        QCOMPARE(result.status, LookupStatus::Denied);
        QVERIFY(result.maintenanceTypes.isEmpty());
    }

    void aFailedReadReportsWhy()
    {
        m_reference.maintenanceTypesError = QStringLiteral("sin conexión");
        const application::MaintenanceFormLookupsResult result =
            service().formLookups(UserRole::Admin);

        QCOMPARE(result.status, LookupStatus::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
        QVERIFY(result.maintenanceTypes.isEmpty());
    }

    // --- Historial ---

    void historyOfAFolio()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        m_maintenances.byFolio.insert(7, {item});

        const application::MaintenanceHistoryResult result = service().history(7, UserRole::Admin);
        QCOMPARE(result.status, HistoryStatus::Loaded);
        QCOMPARE(result.items.size(), 1);
        QCOMPARE(result.items.first().id, 3);
    }

    void sellerDoesNotReadTheHistory()
    {
        m_maintenances.byFolio.insert(7, {application::MaintenanceItemDto()});
        const application::MaintenanceHistoryResult result =
            service().history(7, UserRole::SalesAgent);

        QCOMPARE(result.status, HistoryStatus::Denied);
        QVERIFY(result.items.isEmpty());
    }

    void aFailedHistoryReportsWhy()
    {
        m_maintenances.failWith = QStringLiteral("sin conexión");
        const application::MaintenanceHistoryResult result = service().history(7, UserRole::Admin);

        QCOMPARE(result.status, HistoryStatus::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
    }

    // --- Alta (#46) ---

    void addSavesAndReturnsTheNewTotal()
    {
        m_maintenances.totals.insert(7, 500.0);
        const application::MaintenanceAddResult result = service().add(captured(), UserRole::Admin);

        QCOMPARE(result.status, AddStatus::Saved);
        QCOMPARE(result.id, 100);
        QCOMPARE(result.maintenanceCostTotal, std::optional(2000.50));
        QCOMPARE(m_maintenances.added.size(), 1);
        QCOMPARE(m_maintenances.added.first().vehicleFolio(), 7);
        QCOMPARE(m_maintenances.added.first().cost(), QStringLiteral("1500.50"));
    }

    void aUnitWithoutDealRowStillSavesWithoutTotal()
    {
        const application::MaintenanceAddResult result = service().add(captured(), UserRole::Admin);

        QCOMPARE(result.status, AddStatus::Saved);
        QVERIFY(!result.maintenanceCostTotal);
    }

    void anInvalidCaptureNeverReachesTheDatabase()
    {
        application::NewMaintenanceDto dto = captured(QStringLiteral("10.505"));
        dto.maintenanceTypeId.reset();
        const application::MaintenanceAddResult result = service().add(dto, UserRole::Admin);

        QCOMPARE(result.status, AddStatus::Rejected);
        QCOMPARE(result.validation.fields(),
                 QStringList({QStringLiteral("maintenanceType"), QStringLiteral("cost")}));
        QVERIFY(m_maintenances.added.isEmpty());
    }

    void sellerCannotAdd()
    {
        const application::MaintenanceAddResult result =
            service().add(captured(), UserRole::SalesAgent);

        QCOMPARE(result.status, AddStatus::Denied);
        QVERIFY(m_maintenances.added.isEmpty());
    }

    void aFailedSaveReportsWhy()
    {
        m_maintenances.failWith = QStringLiteral("violación de llave foránea");
        const application::MaintenanceAddResult result = service().add(captured(), UserRole::Admin);

        QCOMPARE(result.status, AddStatus::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("violación de llave foránea"));
    }

    // --- Recibo (#47) ---

    void adminGetsTheReceipt()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        const auto outcome =
            service().generateReceipt(item, QStringLiteral("C:/r.pdf"), UserRole::Admin);

        QVERIFY(outcome.ok);
        QCOMPARE(m_receipts.generated.size(), 1);
        QCOMPARE(m_receipts.generated.first().id, 3);
        QCOMPARE(m_receipts.paths, QStringList{QStringLiteral("C:/r.pdf")});
    }

    void sellerCannotPrintAReceipt()
    {
        const auto outcome = service().generateReceipt(application::MaintenanceItemDto(),
                                                       QStringLiteral("C:/r.pdf"),
                                                       UserRole::SalesAgent);

        QVERIFY(!outcome.ok);
        QVERIFY(!outcome.errorMessage.isEmpty());
        QVERIFY(m_receipts.generated.isEmpty());
    }

    void theDescriptionIsCorrectedTrimmed()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        m_maintenances.byFolio.insert(7, {item});

        const auto result = service().updateDescription(3, QStringLiteral(" Balanceo "),
                                                        UserRole::Admin);

        QCOMPARE(result.status, application::MaintenanceDescriptionUpdateResult::Status::Updated);
        QCOMPARE(m_maintenances.byFolio.value(7).first().description, QStringLiteral("Balanceo"));
    }

    void anEmptyDescriptionIsRejectedBeforeTheDatabase()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        item.description = QStringLiteral("Afinación");
        m_maintenances.byFolio.insert(7, {item});

        const auto result = service().updateDescription(3, QStringLiteral("  "), UserRole::Admin);

        QCOMPARE(result.status, application::MaintenanceDescriptionUpdateResult::Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("description")});
        QCOMPARE(m_maintenances.byFolio.value(7).first().description, QStringLiteral("Afinación"));
    }

    void sellerCannotCorrectADescription()
    {
        const auto result =
            service().updateDescription(3, QStringLiteral("Balanceo"), UserRole::SalesAgent);
        QCOMPARE(result.status, application::MaintenanceDescriptionUpdateResult::Status::Denied);
    }

    void aFailedCorrectionReportsWhy()
    {
        m_maintenances.failWith = QStringLiteral("sin conexión");
        const auto result =
            service().updateDescription(3, QStringLiteral("Balanceo"), UserRole::Admin);
        QCOMPARE(result.status, application::MaintenanceDescriptionUpdateResult::Status::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
    }

    void markingTheReceiptPrintedIsSaved()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        m_maintenances.byFolio.insert(7, {item});

        const auto result = service().markReceiptPrinted(3, UserRole::Admin);

        QCOMPARE(result.status, application::MaintenanceReceiptMarkResult::Status::Marked);
        QVERIFY(service().history(7, UserRole::Admin).items.first().receiptPrinted);
    }

    void sellerCannotMarkAReceipt()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        m_maintenances.byFolio.insert(7, {item});

        const auto result = service().markReceiptPrinted(3, UserRole::SalesAgent);

        QCOMPARE(result.status, application::MaintenanceReceiptMarkResult::Status::Denied);
        QVERIFY(m_maintenances.markedPrinted.isEmpty());
    }

    void aFailedMarkReportsWhy()
    {
        m_maintenances.failWith = QStringLiteral("sin conexión");
        const auto result = service().markReceiptPrinted(3, UserRole::Admin);

        QCOMPARE(result.status, application::MaintenanceReceiptMarkResult::Status::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
    }
};

QTEST_APPLESS_MAIN(TstMaintenanceService)
#include "tst_maintenanceservice.moc"
