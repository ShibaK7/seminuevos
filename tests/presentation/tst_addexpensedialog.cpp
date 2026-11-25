// "Agregar Gasto" y la tabla de Finanzas con widgets reales: la fecha de hoy,
// la descripción del tipo elegido, el No. Factura que aparece con la casilla,
// el IVA y el total calculados, lo que entrega Aceptar, la columna Método de
// Pago y qué celdas de la tabla se editan.

#include "fakes.h"
#include "presentation/finance/addexpensedialog.h"
#include "presentation/finance/financeview.h"
#include "presentation/shell/mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTextEdit>
#include <QtTest>

namespace {

QList<application::ExpenseTypeOptionDto> types()
{
    return {{1, QStringLiteral("Renta"), QStringLiteral("Renta del local.")},
            {4, QStringLiteral("Papelería"), QStringLiteral("Hojas y tóner.")}};
}

template <typename T>
T *child(QWidget &root, const char *name)
{
    return root.findChild<T *>(QString::fromLatin1(name));
}

} // namespace

class TstAddExpenseDialog : public QObject
{
    Q_OBJECT

private slots:
    void opensWithTodayAndNothingChosen()
    {
        AddExpenseDialog dialog(types());

        QCOMPARE(child<QDateEdit>(dialog, "expenseDate")->date(), QDate::currentDate());
        QCOMPARE(child<QComboBox>(dialog, "expenseTypeCombo")->currentIndex(), -1);
        QCOMPARE(child<QComboBox>(dialog, "paymentMethodCombo")->currentIndex(), -1);
        QCOMPARE(child<QComboBox>(dialog, "paymentMethodCombo")->count(), 2);
        QVERIFY(child<QTextEdit>(dialog, "expenseDescription")->isReadOnly());
    }

    void choosingATypeShowsItsDescription()
    {
        AddExpenseDialog dialog(types());

        child<QComboBox>(dialog, "expenseTypeCombo")->setCurrentIndex(1);

        QCOMPARE(child<QTextEdit>(dialog, "expenseDescription")->toPlainText(),
                 QStringLiteral("Hojas y tóner."));
    }

    void theInvoiceNumberAppearsOnlyWithTheCheck()
    {
        AddExpenseDialog dialog(types());
        QLineEdit *number = child<QLineEdit>(dialog, "invoiceNumber");
        QVERIFY(!number->isVisibleTo(&dialog));

        child<QCheckBox>(dialog, "invoiceCheck")->setChecked(true);
        QVERIFY(number->isVisibleTo(&dialog));

        child<QCheckBox>(dialog, "invoiceCheck")->setChecked(false);
        QVERIFY(!number->isVisibleTo(&dialog));
    }

    void ivaAndTotalFollowTheSubtotal()
    {
        AddExpenseDialog dialog(types());

        child<QLineEdit>(dialog, "subtotal")->setText(QStringLiteral("1500.5"));
        QCOMPARE(child<QLineEdit>(dialog, "ivaValue")->text(), QStringLiteral("$240.08"));
        QCOMPARE(child<QLineEdit>(dialog, "totalValue")->text(), QStringLiteral("$1,740.58"));

        child<QLineEdit>(dialog, "subtotal")->setText(QStringLiteral("mil"));
        QVERIFY(child<QLineEdit>(dialog, "ivaValue")->text().isEmpty());
        QVERIFY(child<QLineEdit>(dialog, "totalValue")->text().isEmpty());
    }

    void acceptHandsOverWhatWasCaptured()
    {
        AddExpenseDialog dialog(types());
        QSignalSpy submitted(&dialog, &AddExpenseDialog::submitted);
        child<QComboBox>(dialog, "expenseTypeCombo")->setCurrentIndex(1);
        child<QCheckBox>(dialog, "invoiceCheck")->setChecked(true);
        child<QLineEdit>(dialog, "invoiceNumber")->setText(QStringLiteral("F-9"));
        child<QLineEdit>(dialog, "subtotal")->setText(QStringLiteral("200"));
        QComboBox *payment = child<QComboBox>(dialog, "paymentMethodCombo");
        payment->setCurrentIndex(payment->findText(QStringLiteral("Transferencia")));

        child<QPushButton>(dialog, "expenseAcceptButton")->click();

        QCOMPARE(submitted.size(), 1);
        const auto dto = submitted.first().first().value<application::NewOperationalExpenseDto>();
        QCOMPARE(dto.expenseTypeId, std::optional<int>(4));
        QVERIFY(dto.hasInvoice);
        QCOMPARE(dto.invoiceNumber, QStringLiteral("F-9"));
        QCOMPARE(dto.subtotal, QStringLiteral("200"));
        QCOMPARE(dto.paymentMethod,
                 std::optional<domain::PaymentMethod>(domain::PaymentMethod::Transferencia));
        // Aceptar no cierra: eso lo decide quien guarda.
        QCOMPARE(dialog.result(), 0);
    }

    void withoutTheCheckTheNumberIsNotHandedOver()
    {
        AddExpenseDialog dialog(types());
        child<QCheckBox>(dialog, "invoiceCheck")->setChecked(true);
        child<QLineEdit>(dialog, "invoiceNumber")->setText(QStringLiteral("F-9"));
        child<QCheckBox>(dialog, "invoiceCheck")->setChecked(false);

        QVERIFY(dialog.expense().invoiceNumber.isEmpty());
        QVERIFY(!dialog.expense().hasInvoice);
    }

    void errorsAreMarkedUnderTheirField()
    {
        AddExpenseDialog dialog(types());
        dialog.showErrors({{QStringLiteral("subtotal"), QStringLiteral("Captura el subtotal.")}});

        QVERIFY(child<QLineEdit>(dialog, "subtotal")->property("hasError").toBool());
    }

    void theTableEndsWithPaymentMethodAndOnlyEditsTheInvoice()
    {
        FinanceView view;
        QTableWidget *table = child<QTableWidget>(view, "expensesTable");
        QCOMPARE(table->columnCount(), FinanceView::PaymentMethodColumn + 1);
        QCOMPARE(table->horizontalHeaderItem(FinanceView::PaymentMethodColumn)->text(),
                 QStringLiteral("Método de Pago"));

        application::OperationalExpenseItemDto withInvoice;
        withInvoice.folio = 1;
        withInvoice.hasInvoice = true;
        withInvoice.invoiceNumber = QStringLiteral("F-1");
        withInvoice.paymentMethod = domain::PaymentMethod::Efectivo;
        application::OperationalExpenseItemDto without;
        without.folio = 2;
        view.showExpenses({withInvoice, without});

        QVERIFY(table->item(0, FinanceView::InvoiceColumn)->flags() & Qt::ItemIsUserCheckable);
        QVERIFY(table->item(0, FinanceView::InvoiceNumberColumn)->flags() & Qt::ItemIsEditable);
        QVERIFY(!(table->item(1, FinanceView::InvoiceNumberColumn)->flags() & Qt::ItemIsEditable));
        QVERIFY(!(table->item(0, FinanceView::DescriptionColumn)->flags() & Qt::ItemIsEditable));
        QCOMPARE(table->item(0, FinanceView::PaymentMethodColumn)->text(),
                 QStringLiteral("Efectivo"));
    }

    void bothFiltersStartOnTodo()
    {
        FinanceView view;
        QCOMPARE(child<QComboBox>(view, "periodCombo")->currentText(), QStringLiteral("Todo"));
        QCOMPARE(child<QComboBox>(view, "periodCombo")->count(), 13);
        QCOMPARE(child<QComboBox>(view, "yearCombo")->currentText(), QStringLiteral("Todo"));
        QVERIFY(!view.filter().month);
        QVERIFY(!view.filter().year);
    }

    void choosingAPeriodAndYearTellsTheFilter()
    {
        FinanceView view;
        view.showYears({2026, 2025});
        QSignalSpy changed(&view, &FinanceView::filterChanged);

        child<QComboBox>(view, "periodCombo")->setCurrentIndex(3); // Marzo
        child<QComboBox>(view, "yearCombo")->setCurrentIndex(2);   // 2025

        QCOMPARE(changed.size(), 2);
        QCOMPARE(view.filter().month, std::optional<int>(3));
        QCOMPARE(view.filter().year, std::optional<int>(2025));
    }

    void theYearsKeepTheChosenOneWhenRepainted()
    {
        FinanceView view;
        QComboBox *years = child<QComboBox>(view, "yearCombo");
        QSignalSpy changed(&view, &FinanceView::filterChanged);

        view.showYears({2026, 2025, 2024});
        QCOMPARE(years->currentText(), QStringLiteral("Todo"));
        QCOMPARE(years->count(), 4);

        years->setCurrentIndex(3); // 2024
        changed.clear();
        view.showYears({2026, 2025, 2024, 2019});
        QCOMPARE(years->currentText(), QStringLiteral("2024"));
        // Repintar no es un cambio del usuario.
        QCOMPARE(changed.size(), 0);

        // El elegido ya no existe: vuelve a "Todo" y lo avisa.
        view.showYears({2026, 2025});
        QCOMPARE(years->currentText(), QStringLiteral("Todo"));
        QCOMPARE(changed.size(), 1);
    }

    void theSummaryShowsTheMonthNameTotalAndChange()
    {
        FinanceView view;
        application::MonthlyExpenseSummaryDto summary;
        summary.year = QDate::currentDate().year();
        summary.month = 11;
        summary.total = 1500.5;
        summary.previousTotal = 1000.0;
        summary.changePercent = 50.05;

        view.showMonthlySummary(summary);

        QCOMPARE(child<QLabel>(view, "totalExpensesTitle")->text(),
                 QStringLiteral("Total Egresos (Noviembre)"));
        QCOMPARE(child<QLabel>(view, "totalExpensesAmount")->text(), QStringLiteral("$1,500.50"));
        QLabel *change = child<QLabel>(view, "totalExpensesChange");
        QCOMPARE(change->text(), QStringLiteral("↑ 50.05%"));
        QCOMPARE(change->property("trend").toString(), QStringLiteral("up"));

        summary.year = 2025;
        summary.month = 3;
        summary.changePercent = -25.0;
        view.showMonthlySummary(summary);
        QCOMPARE(child<QLabel>(view, "totalExpensesTitle")->text(),
                 QStringLiteral("Total Egresos (Marzo 2025)"));
        QCOMPARE(change->text(), QStringLiteral("↓ 25.00%"));
        QCOMPARE(change->property("trend").toString(), QStringLiteral("down"));

        summary.changePercent.reset();
        view.showMonthlySummary(summary);
        QCOMPARE(change->text(), QStringLiteral("↑ Sin gastos el mes anterior"));
    }

    void thePrintButtonSaysImprimir()
    {
        FinanceView view;
        QSignalSpy requested(&view, &FinanceView::printReceiptRequested);
        QPushButton *print = child<QPushButton>(view, "printReceiptButton");
        QVERIFY(print);
        QCOMPARE(print->property("title").toString(), QStringLiteral("Imprimir"));
        QCOMPARE(print->property("leadingIcon").toString(), QStringLiteral(":/icons/printer.png"));

        print->click();

        QCOMPARE(requested.size(), 1);
    }

    void theFinanceMenuCanBeHidden()
    {
        MainWindow window;
        QWidget *item = window.findChild<QWidget *>(QStringLiteral("financeItem"));
        QVERIFY(item->isVisibleTo(&window));

        window.setFinanceAvailable(false);

        QVERIFY(!item->isVisibleTo(&window));
    }

    void withoutInvoiceTheNumberSaysSinFacturaUntilChecked()
    {
        FinanceView view;
        QTableWidget *table = child<QTableWidget>(view, "expensesTable");
        application::OperationalExpenseItemDto without;
        without.folio = 3;
        application::OperationalExpenseItemDto with;
        with.folio = 4;
        with.hasInvoice = true;
        with.invoiceNumber = QStringLiteral("F-4");
        view.showExpenses({without, with});

        QCOMPARE(table->item(0, FinanceView::InvoiceNumberColumn)->text(),
                 QStringLiteral("Sin Factura"));
        QCOMPARE(table->item(1, FinanceView::InvoiceNumberColumn)->text(), QStringLiteral("F-4"));

        table->item(0, FinanceView::InvoiceColumn)->setCheckState(Qt::Checked);

        // Al marcarla, la celda queda vacía para capturar el número.
        QVERIFY(table->item(0, FinanceView::InvoiceNumberColumn)->text().isEmpty());
    }

    void checkingAnInvoiceInTheTableLetsItsNumberBeEditedAndTells()
    {
        FinanceView view;
        QTableWidget *table = child<QTableWidget>(view, "expensesTable");
        application::OperationalExpenseItemDto item;
        item.folio = 7;
        view.showExpenses({item});
        QSignalSpy toggled(&view, &FinanceView::invoiceToggled);

        table->item(0, FinanceView::InvoiceColumn)->setCheckState(Qt::Checked);

        QVERIFY(table->item(0, FinanceView::InvoiceNumberColumn)->flags() & Qt::ItemIsEditable);
        QVERIFY(toggled.wait(1000));
        QCOMPARE(toggled.first().at(0).toInt(), 7);
        QCOMPARE(toggled.first().at(1).toBool(), true);
    }
};

QTEST_MAIN(TstAddExpenseDialog)
#include "tst_addexpensedialog.moc"
