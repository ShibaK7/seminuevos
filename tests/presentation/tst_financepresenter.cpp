// Pruebas del presenter de Finanzas: pinta los gastos, "Agregar Gasto" abre el
// formulario con el catálogo y Aceptar guarda, cierra y repinta (o deja los
// errores en el formulario), y la factura se corrige desde la tabla.

#include "application/finance/services/operationalexpenseservice.h"
#include "fakes.h"
#include "presentation/finance/financepresenter.h"

#include <QtTest>

namespace {

application::NewOperationalExpenseDto captured()
{
    application::NewOperationalExpenseDto dto;
    dto.date = QDate(2026, 11, 24);
    dto.expenseTypeId = 1;
    dto.subtotal = QStringLiteral("1000");
    dto.paymentMethod = domain::PaymentMethod::Efectivo;
    return dto;
}

application::OperationalExpenseItemDto saved(int folio, bool hasInvoice = false,
                                             const QString &number = QString())
{
    application::OperationalExpenseItemDto item;
    item.folio = folio;
    item.hasInvoice = hasInvoice;
    item.invoiceNumber = number;
    return item;
}

} // namespace

class TstFinancePresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryOperationalExpenseRepository m_repository;
    fakes::FakeExpenseReceiptGenerator m_receipts;
    fakes::FakeFinanceView m_view;

private slots:
    void init()
    {
        m_repository = fakes::InMemoryOperationalExpenseRepository();
        m_receipts = fakes::FakeExpenseReceiptGenerator();
        m_view = fakes::FakeFinanceView();
    }

    void reloadPaintsTheExpenses()
    {
        m_repository.items << saved(3);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);

        presenter.reload();

        QCOMPARE(m_view.shownExpenses.size(), 1);
        QCOMPARE(m_view.shownExpenses.first().first().folio, 3);
    }

    void aSellerGetsNothingPaintedNorWarned()
    {
        m_repository.items << saved(3);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::SalesAgent);

        presenter.reload();

        QVERIFY(m_view.shownExpenses.isEmpty());
        QVERIFY(m_view.warnings.isEmpty());
    }

    void aSellerCannotOpenTheForm()
    {
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::SalesAgent);

        presenter.onAddExpense();

        QVERIFY(m_view.openedForms.isEmpty());
        QCOMPARE(m_view.warnings, QStringList{QStringLiteral("Agregar gasto")});
    }

    void aNewExpenseOfANewYearAppearsInTheYearsAfterSaving()
    {
        m_repository.types << application::ExpenseTypeOptionDto{1, QStringLiteral("Renta"),
                                                                QString()};
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        const int current = QDate::currentDate().year();
        QCOMPARE(m_view.shownYears.last(), QList<int>{current});

        application::NewOperationalExpenseDto dto = captured();
        dto.date = QDate(2019, 6, 1);
        presenter.onSubmitExpense(dto);

        // La recarga que sigue al alta trae el año nuevo: nadie lo comparó.
        QCOMPARE(m_view.shownYears.last(), (QList<int>{current, 2019}));
    }

    void changingTheFilterRereadsWithItAndPaintsTheSummary()
    {
        application::OperationalExpenseItemDto march = saved(1);
        march.date = QDate(2025, 3, 5);
        march.total = 300.0;
        application::OperationalExpenseItemDto april = saved(2);
        april.date = QDate(2025, 4, 5);
        m_repository.items << march << april;
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        QCOMPARE(m_view.shownExpenses.last().size(), 2);

        application::OperationalExpenseFilterDto filter;
        filter.month = 3;
        filter.year = 2025;
        presenter.onFilterChanged(filter);

        QCOMPARE(m_repository.lastFilter.month, std::optional<int>(3));
        QCOMPARE(m_view.shownExpenses.last().size(), 1);
        QCOMPARE(m_view.shownExpenses.last().first().folio, 1);
        QCOMPARE(m_view.shownSummaries.last().month, 3);
        QCOMPARE(m_view.shownSummaries.last().total, 300.0);
    }

    void savingKeepsTheFilter()
    {
        m_repository.types << application::ExpenseTypeOptionDto{1, QStringLiteral("Renta"),
                                                                QString()};
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        application::OperationalExpenseFilterDto filter;
        filter.year = 2025;
        presenter.onFilterChanged(filter);

        presenter.onSubmitExpense(captured());

        QCOMPARE(m_repository.lastFilter.year, std::optional<int>(2025));
    }

    void onlyTheLatestReadIsPainted()
    {
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::DeferredTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);

        presenter.reload();
        presenter.reload();
        runner.finish(1);
        runner.finish(0);

        QCOMPARE(m_view.shownExpenses.size(), 1);
    }

    void addOpensTheFormWithTheCatalog()
    {
        m_repository.types << application::ExpenseTypeOptionDto{1, QStringLiteral("Renta"),
                                                                QStringLiteral("Local")};
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);

        presenter.onAddExpense();

        QCOMPARE(m_view.busyChanges, (QList<bool>{true, false}));
        QCOMPARE(m_view.openedForms.size(), 1);
        QCOMPARE(m_view.openedForms.first().first().description, QStringLiteral("Local"));
    }

    void anEmptyCatalogWarnsInsteadOfOpeningTheForm()
    {
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);

        presenter.onAddExpense();

        QVERIFY(m_view.openedForms.isEmpty());
        QCOMPARE(m_view.warnings, QStringList{QStringLiteral("No hay tipos de gasto")});
    }

    void aSecondClickWhileLoadingOpensNothing()
    {
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::DeferredTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);

        presenter.onAddExpense();
        presenter.onAddExpense();

        QCOMPARE(runner.works.size(), 1);
    }

    void submitSavesClosesAndRepaints()
    {
        m_repository.types << application::ExpenseTypeOptionDto{1, QStringLiteral("Renta"),
                                                                QStringLiteral("Local")};
        m_repository.nextFolio = 9;
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);

        presenter.onSubmitExpense(captured());

        QCOMPARE(m_view.formsClosed, 1);
        QCOMPARE(m_view.formBusyChanges, (QList<bool>{true, false}));
        QCOMPARE(m_view.shownExpenses.size(), 1);
        const application::OperationalExpenseItemDto &row = m_view.shownExpenses.first().first();
        QCOMPARE(row.folio, 9);
        QCOMPARE(row.description, QStringLiteral("Local"));
        QCOMPARE(row.iva, 160.0);
    }

    void aRejectedSubmitKeepsTheFormOpenWithItsErrors()
    {
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        application::NewOperationalExpenseDto dto = captured();
        dto.subtotal.clear();

        presenter.onSubmitExpense(dto);

        QCOMPARE(m_view.formsClosed, 0);
        QCOMPARE(m_view.formErrors.size(), 1);
        QCOMPARE(m_view.formErrors.first().first().field, QStringLiteral("subtotal"));
    }

    void checkingAnInvoiceWithoutNumberOpensItsCell()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceToggled(4, true);

        QCOMPARE(m_view.editedInvoiceNumbers, QList<int>{4});
        QCOMPARE(m_repository.invoiceUpdates, 0);
    }

    void typingTheNumberSavesTheInvoice()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceNumberEdited(4, QStringLiteral(" F-10 "));

        QCOMPARE(m_repository.invoiceUpdates, 1);
        QVERIFY(m_repository.items.first().hasInvoice);
        QCOMPARE(m_repository.items.first().invoiceNumber, QStringLiteral("F-10"));
        // Se releyó para pintar lo guardado.
        QCOMPARE(m_view.shownExpenses.size(), 2);
    }

    void anUnchangedNumberSavesNothing()
    {
        m_repository.items << saved(4, true, QStringLiteral("F-10"));
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceNumberEdited(4, QStringLiteral("F-10"));

        QCOMPARE(m_repository.invoiceUpdates, 0);
    }

    void uncheckingASavedInvoiceSavesIt()
    {
        m_repository.items << saved(4, true, QStringLiteral("F-10"));
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceToggled(4, false);

        QCOMPARE(m_repository.invoiceUpdates, 1);
        QVERIFY(!m_repository.items.first().hasInvoice);
        QVERIFY(m_repository.items.first().invoiceNumber.isEmpty());
    }

    void uncheckingAnUnsavedCheckOnlyRepaints()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceToggled(4, false);

        QCOMPARE(m_repository.invoiceUpdates, 0);
        QCOMPARE(m_view.shownExpenses.size(), 2);
    }

    // --- Confirmaciones ---

    void uncheckingAsksFirstAndCancelRevertsWithoutSaving()
    {
        m_repository.items << saved(4, true, QStringLiteral("F-10"));
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.acceptInvoiceUpdate = false;

        presenter.onInvoiceToggled(4, false);

        QCOMPARE(m_view.invoiceConfirmations, 1);
        QCOMPARE(m_repository.invoiceUpdates, 0);
        // Se repintó lo guardado: la casilla vuelve a marcada.
        QCOMPARE(m_view.shownExpenses.size(), 2);
        QVERIFY(m_view.shownExpenses.last().first().hasInvoice);
    }

    void checkingAsksFirstAndCancelDoesNotOpenTheNumber()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.acceptInvoiceUpdate = false;

        presenter.onInvoiceToggled(4, true);

        QCOMPARE(m_view.invoiceConfirmations, 1);
        QVERIFY(m_view.editedInvoiceNumbers.isEmpty());
        QVERIFY(!m_view.shownExpenses.last().first().hasInvoice);
    }

    void theFirstNumberAfterCheckingDoesNotAskAgain()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceToggled(4, true);
        presenter.onInvoiceNumberEdited(4, QStringLiteral("F-10"));

        QCOMPARE(m_view.invoiceConfirmations, 1);
        QCOMPARE(m_view.invoiceNumberConfirmations, 0);
        QCOMPARE(m_repository.invoiceUpdates, 1);
    }

    void replacingASavedNumberAsksFirst()
    {
        m_repository.items << saved(4, true, QStringLiteral("F-10"));
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceNumberEdited(4, QStringLiteral("F-11"));

        QCOMPARE(m_view.invoiceNumberConfirmations, 1);
        QCOMPARE(m_repository.items.first().invoiceNumber, QStringLiteral("F-11"));
    }

    void cancellingANumberReplacementRevertsWithoutSaving()
    {
        m_repository.items << saved(4, true, QStringLiteral("F-10"));
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.acceptInvoiceNumberUpdate = false;

        presenter.onInvoiceNumberEdited(4, QStringLiteral("F-11"));

        QCOMPARE(m_repository.invoiceUpdates, 0);
        QCOMPARE(m_view.shownExpenses.last().first().invoiceNumber, QStringLiteral("F-10"));
    }

    void uncheckingAnUnsavedCheckDoesNotAsk()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceToggled(4, false);

        QCOMPARE(m_view.invoiceConfirmations, 0);
    }

    // --- Número faltante ---

    void leavingTheNumberEmptyAfterCheckingAlertsAndReopensIt()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceToggled(4, true);
        presenter.onInvoiceNumberEdited(4, QStringLiteral("  "));

        QCOMPARE(m_view.missingInvoiceNumberAlerts, 1);
        // Se abrió al marcar y otra vez tras la alerta.
        QCOMPARE(m_view.editedInvoiceNumbers, (QList<int>{4, 4}));
        QCOMPARE(m_repository.invoiceUpdates, 0);
    }

    void leavingWithoutInvoiceUnchecksItWithoutSaving()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.captureMissingInvoiceNumber = false;

        presenter.onInvoiceToggled(4, true);
        presenter.onInvoiceNumberEdited(4, QString());

        QCOMPARE(m_repository.invoiceUpdates, 0);
        QCOMPARE(m_view.editedInvoiceNumbers, QList<int>{4});
        QVERIFY(!m_view.shownExpenses.last().first().hasInvoice);
    }

    void emptyingASavedNumberWarnsAndReverts()
    {
        m_repository.items << saved(4, true, QStringLiteral("F-10"));
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onInvoiceNumberEdited(4, QString());

        QCOMPARE(m_view.invoiceNumberConfirmations, 0);
        QCOMPARE(m_repository.invoiceUpdates, 0);
        QCOMPARE(m_view.warnings, QStringList{QStringLiteral("No se actualizó la factura")});
        QCOMPARE(m_view.shownExpenses.last().first().invoiceNumber, QStringLiteral("F-10"));
    }

    // --- Recibo de pago ---

    void printGeneratesTheReceiptOfTheSelectedRowAndOpensIt()
    {
        application::OperationalExpenseItemDto item = saved(8);
        item.total = 1160.0;
        m_repository.items << saved(7) << item;
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.selectedFolio = 8;

        presenter.onPrintReceipt();

        QCOMPARE(m_view.suggestedReceiptNames,
                 QStringList{QStringLiteral("recibo_gasto_folio8.pdf")});
        QCOMPARE(m_receipts.generated.size(), 1);
        QCOMPARE(m_receipts.generated.first().folio, 8);
        QCOMPARE(m_receipts.generated.first().total, 1160.0);
        QCOMPARE(m_view.openedDocuments, QStringList{m_view.receiptPath});
    }

    void printWithoutSelectionAsksToSelectARow()
    {
        m_repository.items << saved(7);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();

        presenter.onPrintReceipt();

        QCOMPARE(m_view.warnings, QStringList{QStringLiteral("Imprimir recibo")});
        QVERIFY(m_receipts.generated.isEmpty());
    }

    void cancellingTheSaveDialogPrintsNothing()
    {
        m_repository.items << saved(7);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.selectedFolio = 7;
        m_view.receiptPath.clear();

        presenter.onPrintReceipt();

        QVERIFY(m_receipts.generated.isEmpty());
        QVERIFY(m_view.openedDocuments.isEmpty());
    }

    void aFailedReceiptWarnsAndOpensNothing()
    {
        m_repository.items << saved(7);
        m_receipts.failWith = QStringLiteral("sin plantilla");
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_view.selectedFolio = 7;

        presenter.onPrintReceipt();

        QCOMPARE(m_view.warnings, QStringList{QStringLiteral("Error al generar el recibo")});
        QVERIFY(m_view.openedDocuments.isEmpty());
    }

    void aFailedInvoiceUpdateWarnsAndRevertsTheTable()
    {
        m_repository.items << saved(4);
        const application::OperationalExpenseService service(m_repository, m_receipts);
        fakes::InlineTaskRunner runner;
        presentation::FinancePresenter presenter(m_view, service, runner,
                                                 domain::UserRole::Admin);
        presenter.reload();
        m_repository.failWith = QStringLiteral("sin conexión");

        presenter.onInvoiceNumberEdited(4, QStringLiteral("F-10"));

        QCOMPARE(m_view.warnings, QStringList{QStringLiteral("No se actualizó la factura")});
        QCOMPARE(m_view.shownExpenses.size(), 2);
        QVERIFY(!m_view.shownExpenses.last().first().hasInvoice);
    }
};

QTEST_GUILESS_MAIN(TstFinancePresenter)
#include "tst_financepresenter.moc"
