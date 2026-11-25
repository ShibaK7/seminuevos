// Pruebas del caso de uso de los gastos operativos: el alta valida antes de
// tocar la base y guarda el IVA que calcula el dominio, y corregir la factura
// pide el número cuando la casilla queda marcada.

#include "application/finance/services/operationalexpenseservice.h"
#include "fakes.h"

#include <QtTest>

namespace {

constexpr domain::UserRole kAdmin = domain::UserRole::Admin;

application::NewOperationalExpenseDto captured()
{
    application::NewOperationalExpenseDto dto;
    dto.date = QDate(2026, 11, 24);
    dto.expenseTypeId = 1;
    dto.hasInvoice = true;
    dto.invoiceNumber = QStringLiteral("F-77");
    dto.subtotal = QStringLiteral("1000");
    dto.paymentMethod = domain::PaymentMethod::Transferencia;
    return dto;
}

} // namespace

class TstOperationalExpenseService : public QObject
{
    Q_OBJECT

private slots:
    void addSavesWithTheIvaOfTheDomain()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        repository.nextFolio = 41;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);

        const application::OperationalExpenseAddResult result = service.add(captured(), kAdmin);

        QCOMPARE(result.status, application::OperationalExpenseAddResult::Status::Saved);
        QCOMPARE(result.folio, 41);
        QCOMPARE(repository.added.size(), 1);
        QCOMPARE(repository.added.first().amounts()->iva(), QStringLiteral("160.00"));
        QCOMPARE(repository.added.first().amounts()->total(), QStringLiteral("1160.00"));
    }

    void addRejectsWithoutTouchingTheDatabase()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);
        application::NewOperationalExpenseDto dto = captured();
        dto.expenseTypeId.reset();
        dto.invoiceNumber.clear();

        const application::OperationalExpenseAddResult result = service.add(dto, kAdmin);

        QCOMPARE(result.status, application::OperationalExpenseAddResult::Status::Rejected);
        QCOMPARE(result.validation.fields(),
                 (QStringList{QStringLiteral("expenseType"), QStringLiteral("invoiceNumber")}));
        QVERIFY(repository.added.isEmpty());
    }

    void addReportsADatabaseFailure()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        repository.failWith = QStringLiteral("sin conexión");
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);

        const application::OperationalExpenseAddResult result = service.add(captured(), kAdmin);

        QCOMPARE(result.status, application::OperationalExpenseAddResult::Status::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
    }

    void anInvoiceWithoutNumberIsRejected()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        repository.items << application::OperationalExpenseItemDto{};
        repository.items.first().folio = 5;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);

        const auto result = service.updateInvoice(5, true, QStringLiteral("  "), kAdmin);

        QCOMPARE(result.status, application::OperationalExpenseInvoiceUpdateResult::Status::Rejected);
        QCOMPARE(repository.invoiceUpdates, 0);
    }

    void removingTheInvoiceClearsItsNumber()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        application::OperationalExpenseItemDto item;
        item.folio = 5;
        item.hasInvoice = true;
        item.invoiceNumber = QStringLiteral("F-1");
        repository.items << item;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);

        const auto result = service.updateInvoice(5, false, QStringLiteral("F-1"), kAdmin);

        QCOMPARE(result.status, application::OperationalExpenseInvoiceUpdateResult::Status::Updated);
        QVERIFY(!repository.items.first().hasInvoice);
        QVERIFY(repository.items.first().invoiceNumber.isEmpty());
    }

    void aSellerIsDeniedEveryOperation()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        application::OperationalExpenseItemDto item;
        item.folio = 5;
        repository.items << item;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);
        const domain::UserRole seller = domain::UserRole::SalesAgent;

        QCOMPARE(service.formLookups(seller).status,
                 application::ExpenseFormLookupsResult::Status::Denied);
        QCOMPARE(service.list({}, seller).status,
                 application::OperationalExpenseListResult::Status::Denied);
        QCOMPARE(service.add(captured(), seller).status,
                 application::OperationalExpenseAddResult::Status::Denied);
        QCOMPARE(service.updateInvoice(5, true, QStringLiteral("F-1"), seller).status,
                 application::OperationalExpenseInvoiceUpdateResult::Status::Denied);
        // Nada llegó a la base.
        QVERIFY(repository.added.isEmpty());
        QCOMPARE(repository.invoiceUpdates, 0);
    }

    void theReceiptIsGeneratedOnlyForTheAdmin()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);
        application::OperationalExpenseItemDto item;
        item.folio = 3;

        QVERIFY(!service.generateReceipt(item, QStringLiteral("a.pdf"),
                                         domain::UserRole::SalesAgent).ok);
        QVERIFY(receipts.generated.isEmpty());

        QVERIFY(service.generateReceipt(item, QStringLiteral("a.pdf"), kAdmin).ok);
        QCOMPARE(receipts.generated.size(), 1);
        QCOMPARE(receipts.paths, QStringList{QStringLiteral("a.pdf")});
    }

    void listBringsTheYearsMostRecentFirstWithTheCurrentOne()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        for (const QDate &date : {QDate(2022, 3, 1), QDate(2024, 5, 2), QDate(2022, 9, 9)}) {
            application::OperationalExpenseItemDto item;
            item.date = date;
            repository.items << item;
        }
        const application::OperationalExpenseService service(repository, receipts);

        const auto result = service.list({}, kAdmin);

        const int current = QDate::currentDate().year();
        QCOMPARE(result.status, application::OperationalExpenseListResult::Status::Loaded);
        // Sin repetir 2022, y el año en curso aunque no tenga gastos.
        QCOMPARE(result.years, (QList<int>{current, 2024, 2022}));
    }

    void anEmptyBaseStillHasTheCurrentYear()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);

        QCOMPARE(service.list({}, kAdmin).years, QList<int>{QDate::currentDate().year()});
    }

    void failingToReadTheYearsFailsTheList()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        repository.failYearsWith = QStringLiteral("sin conexión");
        const application::OperationalExpenseService service(repository, receipts);

        const auto result = service.list({}, kAdmin);

        QCOMPARE(result.status, application::OperationalExpenseListResult::Status::Failed);
        QVERIFY(result.items.isEmpty());
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
    }

    void theFilterReachesTheRepositoryAndFiltersTheRows()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        for (const QDate &date : {QDate(2025, 3, 5), QDate(2025, 4, 1), QDate(2024, 3, 9)}) {
            application::OperationalExpenseItemDto item;
            item.date = date;
            repository.items << item;
        }
        const application::OperationalExpenseService service(repository, receipts);

        application::OperationalExpenseFilterDto march2025;
        march2025.month = 3;
        march2025.year = 2025;
        QCOMPARE(service.list(march2025, kAdmin).items.size(), 1);

        application::OperationalExpenseFilterDto anyMarch;
        anyMarch.month = 3;
        QCOMPARE(service.list(anyMarch, kAdmin).items.size(), 2);

        application::OperationalExpenseFilterDto year2025;
        year2025.year = 2025;
        QCOMPARE(service.list(year2025, kAdmin).items.size(), 2);
        // Los años del combo salen de toda la tabla, no del filtro.
        QVERIFY(service.list(year2025, kAdmin).years.contains(2024));
    }

    void theSummaryComparesTheFilteredMonthWithThePreviousOne()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        const QList<std::pair<QDate, double>> rows = {
            {QDate(2025, 3, 5), 600.0}, {QDate(2025, 3, 31), 600.0},
            {QDate(2025, 2, 10), 1000.0}, {QDate(2025, 4, 1), 9999.0}};
        for (const auto &[date, total] : rows) {
            application::OperationalExpenseItemDto item;
            item.date = date;
            item.total = total;
            repository.items << item;
        }
        const application::OperationalExpenseService service(repository, receipts);
        application::OperationalExpenseFilterDto march2025;
        march2025.month = 3;
        march2025.year = 2025;

        const application::MonthlyExpenseSummaryDto summary =
            service.list(march2025, kAdmin).summary;

        QCOMPARE(summary.year, 2025);
        QCOMPARE(summary.month, 3);
        QCOMPARE(summary.total, 1200.0);
        QCOMPARE(summary.previousTotal, 1000.0);
        QCOMPARE(summary.changePercent, std::optional<double>(20.0));
    }

    void withTodoTheSummaryIsTheCurrentMonth()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        fakes::FakeExpenseReceiptGenerator receipts;
        application::OperationalExpenseItemDto item;
        item.date = QDate::currentDate();
        item.total = 500.0;
        repository.items << item;
        const application::OperationalExpenseService service(repository, receipts);
        // Solo el mes: falta el año, así que la tarjeta sigue en el mes actual.
        application::OperationalExpenseFilterDto onlyMonth;
        onlyMonth.month = 1;

        for (const auto &filter : {application::OperationalExpenseFilterDto{}, onlyMonth}) {
            const auto summary = service.list(filter, kAdmin).summary;
            QCOMPARE(summary.year, QDate::currentDate().year());
            QCOMPARE(summary.month, QDate::currentDate().month());
            QCOMPARE(summary.total, 500.0);
        }
    }

    void lookupsReportADatabaseFailure()
    {
        fakes::InMemoryOperationalExpenseRepository repository;
        repository.failWith = QStringLiteral("sin conexión");
        fakes::FakeExpenseReceiptGenerator receipts;
        const application::OperationalExpenseService service(repository, receipts);

        QCOMPARE(service.formLookups(kAdmin).status, application::ExpenseFormLookupsResult::Status::Failed);
        QCOMPARE(service.list({}, kAdmin).status, application::OperationalExpenseListResult::Status::Failed);
    }
};

QTEST_APPLESS_MAIN(TstOperationalExpenseService)
#include "tst_operationalexpenseservice.moc"
