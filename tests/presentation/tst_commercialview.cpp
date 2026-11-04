#include "application/commercial/services/commercialservice.h"
#include "presentation/commercial/commercialmainview.h"
#include "presentation/commercial/commercialpresenter.h"
#include "presentation/commercial/saledetails/addpaymentdialog.h"
#include "presentation/commercial/saledetails/SaleDetails.h"
#include "presentation/commercial/saledetails/saledetailspresenter.h"
#include "presentation/commercial/registration/saledetailsview.h"
#include "presentation/shell/mainwindow.h"
#include "presentation/common/tasks/taskrunner.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTimer>
#include <QTableWidget>
#include <QTableView>
#include <QHeaderView>
#include <QStackedWidget>
#include <QtTest>

namespace {

class InlineTaskRunner final : public presentation::TaskRunner
{
protected:
    void submit(std::function<void()> work, std::function<void()> done) override
    {
        work();
        done();
    }
};

class FakeCommercialReader final : public application::CommercialReader,
                                   public application::CommercialPaymentWriter
{
public:
    application::CommercialFilterDto lastFilter;
    application::CommercialSaleDetailsDto details;
    application::CommercialSaleRegistrationDetailsDto registrationDetails;
    application::CommercialPaymentLookupResultDto nextPayment;
    int registrationDetailReads = 0;
    int waiverUpdates = 0;
    int nextPaymentReads = 0;
    int paymentWrites = 0;
    int lastPaymentFolio = -1;
    bool lastWaiverValue = false;
    application::CommercialPaymentDto lastRecordedPayment;

    QList<application::CommercialSaleDto> search(
        const application::CommercialFilterDto &filter, QString *) override
    {
        lastFilter = filter;
        application::CommercialSaleDto sale;
        sale.folio = 42;
        sale.saleDate = QDate(2026, 6, 12);
        sale.buyerName = QStringLiteral("Ana López");
        sale.model = QStringLiteral("Versa");
        sale.yearModel = 2022;
        sale.paymentType = QStringLiteral("Contado");
        sale.totalAmount = 250000.0;
        sale.status = QStringLiteral("Pago Completo");
        sale.paidAmount = 200000.0;
        sale.balanceDue = 50000.0;
        return {sale};
    }

    application::CommercialSaleDetailsDto saleDetails(int, QString *) override
    {
        return details;
    }

    application::CommercialPaymentLookupResultDto nextUnpaidPayment(int saleFolio) override
    {
        ++nextPaymentReads;
        if (nextPayment.payment.saleFolio != saleFolio)
            return {};
        return nextPayment;
    }

    application::CommercialSaleRegistrationDetailsDto saleRegistrationDetails(
        int, QString *) override
    {
        ++registrationDetailReads;
        return registrationDetails;
    }

    bool updatePaymentWaiver(int paymentFolio, bool waived, domain::UserRole role,
                             QString *error) override
    {
        if (role != domain::UserRole::Admin) {
            if (error)
                *error = QStringLiteral("Solo un Administrador puede modificar condonaciones.");
            return false;
        }
        ++waiverUpdates;
        lastPaymentFolio = paymentFolio;
        lastWaiverValue = waived;
        return true;
    }

    bool recordPayment(const application::CommercialPaymentDto &payment,
                       QString *error) override
    {
        ++paymentWrites;
        lastRecordedPayment = payment;
        if (error)
            error->clear();
        return true;
    }
};

} // namespace

class TstCommercialView : public QObject
{
    Q_OBJECT

private slots:
    void paymentEntryDialogValidatesAndEmitsPaymentFields()
    {
        application::CommercialPaymentDto payment;
        payment.folio = 550;
        payment.saleFolio = 123;
        payment.installmentNumber = 4;
        payment.scheduledDate = QDate::currentDate().addDays(10);
        payment.expectedAmount = 2500.0;

        AddPaymentDialog dialog(payment);
        dialog.show();
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("saleFolioLabel"))->text(),
                 QStringLiteral("123"));
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("installmentNumberLabel"))->text(),
                 QStringLiteral("4"));
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("expectedAmountLabel"))->text(),
                 QLocale(QLocale::Spanish, QLocale::Mexico)
                     .toCurrencyString(2500.0, QStringLiteral("$"), 2));
        auto *scheduledDate = dialog.findChild<QLabel *>(
            QStringLiteral("scheduledDateLabel"));
        QVERIFY(scheduledDate);
        QCOMPARE(scheduledDate->text(), payment.scheduledDate.toString(QStringLiteral("dd/MM/yyyy")));
        QVERIFY(!dialog.findChild<QDateEdit *>(QStringLiteral("scheduledDateEdit")));
        auto *paidDate = dialog.findChild<QDateTimeEdit *>(QStringLiteral("paidDateEdit"));
        QVERIFY(paidDate);
        QCOMPARE(paidDate->minimumDateTime().date(), QDate::currentDate().addMonths(-2));
        QCOMPARE(paidDate->maximumDateTime().date(), QDate::currentDate());

        auto *accept = dialog.findChild<QPushButton *>(QStringLiteral("acceptPaymentButton"));
        auto *amount = dialog.findChild<QLineEdit *>(QStringLiteral("paidAmountEdit"));
        auto *method = dialog.findChild<QLineEdit *>(QStringLiteral("paymentMethodEdit"));
        auto *lateFee = dialog.findChild<QLineEdit *>(QStringLiteral("lateFeeEdit"));
        auto *status = dialog.findChild<QComboBox *>(QStringLiteral("paymentStatusCombo"));
        QVERIFY(accept && amount && method && lateFee && status);
        QCOMPARE(status->count(), 4);
        QCOMPARE(status->currentText(), QStringLiteral("Pendiente"));

        application::CommercialPaymentDto submittedPayment;
        int submissions = 0;
        connect(&dialog, &AddPaymentDialog::submitted, &dialog,
                [&submittedPayment, &submissions](
                    const application::CommercialPaymentDto &value) {
                    submittedPayment = value;
                    ++submissions;
                });
        accept->click();
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("paidAmountError"))->isHidden());
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("paymentMethodError"))->isHidden());
        QCOMPARE(submissions, 0);

        amount->setText(QStringLiteral("1250.755"));
        QVERIFY(!amount->hasAcceptableInput());
        amount->setText(QStringLiteral("1250.75"));
        lateFee->setText(QStringLiteral("15.25"));
        method->setText(QStringLiteral("Transferencia"));
        status->setCurrentText(QStringLiteral("Parcial"));
        dialog.findChild<QLineEdit *>(QStringLiteral("referenceNumberEdit"))
            ->setText(QStringLiteral("REF-123"));
        dialog.findChild<QPlainTextEdit *>(QStringLiteral("paymentNotesEdit"))
            ->setPlainText(QStringLiteral("Pago de prueba"));
        accept->click();

        QCOMPARE(submissions, 1);
        QCOMPARE(submittedPayment.folio, 550);
        QCOMPARE(submittedPayment.saleFolio, 123);
        QCOMPARE(submittedPayment.installmentNumber, 4);
        QCOMPARE(submittedPayment.paidAmount, 1250.75);
        QCOMPARE(submittedPayment.lateFee, 15.25);
        QCOMPARE(submittedPayment.paymentMethod, QStringLiteral("Transferencia"));
        QCOMPARE(submittedPayment.status, QStringLiteral("Parcial"));
        QCOMPARE(submittedPayment.referenceNumber, QStringLiteral("REF-123"));
        QCOMPARE(submittedPayment.notes, QStringLiteral("Pago de prueba"));
        QVERIFY(!accept->isEnabled());
    }

    void addPaymentButtonLoadsAndSavesTheNextUnpaidPayment()
    {
        application::CommercialSaleDetailsDto details;
        details.folio = 123;
        application::CommercialPaymentDto unpaid;
        unpaid.folio = 923;
        unpaid.saleFolio = details.folio;
        unpaid.installmentNumber = 5;
        unpaid.scheduledDate = QDate::currentDate().addDays(15);
        unpaid.expectedAmount = 1800.0;

        FakeCommercialReader reader;
        reader.details = details;
        reader.nextPayment.payment = unpaid;
        reader.nextPayment.found = true;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        SaleDetails view(domain::UserRole::Admin);
        presentation::CommercialSaleDetailsPresenter presenter(
            view, service, runner, details.folio, domain::UserRole::Admin);
        view.bind(presenter);
        presenter.load();

        QTimer::singleShot(0, &view, [&view] {
            auto *dialog = view.findChild<AddPaymentDialog *>(
                QStringLiteral("addPaymentDialog"));
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>(QStringLiteral("paidAmountEdit"))
                ->setText(QStringLiteral("1700.00"));
            dialog->findChild<QLineEdit *>(QStringLiteral("paymentMethodEdit"))
                ->setText(QStringLiteral("Efectivo"));
            dialog->findChild<QComboBox *>(QStringLiteral("paymentStatusCombo"))
                ->setCurrentText(QStringLiteral("Pagado"));
            dialog->findChild<QPushButton *>(QStringLiteral("acceptPaymentButton"))->click();
        });

        view.findChild<QPushButton *>(QStringLiteral("addPaymentButton"))->click();

        QCOMPARE(reader.nextPaymentReads, 1);
        QCOMPARE(reader.paymentWrites, 1);
        QCOMPARE(reader.lastRecordedPayment.folio, unpaid.folio);
        QCOMPARE(reader.lastRecordedPayment.saleFolio, details.folio);
        QCOMPARE(reader.lastRecordedPayment.installmentNumber, unpaid.installmentNumber);
        QCOMPARE(reader.lastRecordedPayment.paidAmount, 1700.0);
        QCOMPARE(reader.lastRecordedPayment.paymentMethod, QStringLiteral("Efectivo"));
        QCOMPARE(reader.lastRecordedPayment.status, QStringLiteral("Pagado"));
    }

    void paymentServiceRejectsInvalidAmountsBeforePersistence()
    {
        FakeCommercialReader reader;
        application::CommercialService service(reader, reader);
        application::CommercialPaymentDto payment;
        payment.folio = 550;
        payment.saleFolio = 123;
        payment.installmentNumber = 4;
        payment.paidDate = QDate::currentDate();
        payment.paymentMethod = QStringLiteral("Efectivo");
        payment.status = QStringLiteral("Pagado");
        payment.paidAmount = 0.0;

        const auto zeroAmount = service.recordPayment(payment);
        QVERIFY(!zeroAmount.ok);
        QVERIFY(!zeroAmount.errorMessage.isEmpty());
        QCOMPARE(reader.paymentWrites, 0);

        payment.paidAmount = 100.0;
        payment.lateFee = -0.01;
        const auto negativeLateFee = service.recordPayment(payment);
        QVERIFY(!negativeLateFee.ok);
        QCOMPARE(reader.paymentWrites, 0);

        payment.lateFee = 0.0;
        payment.paidAmount = 100.001;
        const auto excessiveDecimals = service.recordPayment(payment);
        QVERIFY(!excessiveDecimals.ok);
        QCOMPARE(reader.paymentWrites, 0);
    }

    void loadsSalesIntoTheRequestedColumns()
    {
        CommercialMainView view;
        FakeCommercialReader reader;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        presentation::CommercialPresenter presenter(view, service, runner);
        view.bind(presenter);

        presenter.reload();

        auto *table = view.findChild<QTableWidget *>(QStringLiteral("tableWidget"));
        QVERIFY(table);
        QCOMPARE(table->columnCount(), 8);
        QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("Folio"));
        QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("Fecha de Venta"));
        QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Comprador"));
        QCOMPARE(table->horizontalHeaderItem(3)->text(), QStringLiteral("Vehículo (Modelo/Año)"));
        QCOMPARE(table->horizontalHeaderItem(4)->text(), QStringLiteral("Tipo de Pago"));
        QCOMPARE(table->horizontalHeaderItem(5)->text(), QStringLiteral("Monto Total"));
        QCOMPARE(table->horizontalHeaderItem(6)->text(), QStringLiteral("Utilidad"));
        QCOMPARE(table->horizontalHeaderItem(7)->text(), QStringLiteral("Estado"));

        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("42"));
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("12/06/2026"));
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Ana López"));
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("Versa/2022"));
        QCOMPARE(table->item(0, 4)->text(), QStringLiteral("Contado"));
        QCOMPARE(table->item(0, 5)->data(Qt::UserRole).toDouble(), 250000.0);
        QCOMPARE(table->item(0, 7)->text(), QStringLiteral("Pago Completo"));
        QCOMPARE(table->item(0, 6)->text(), QStringLiteral("TBD"));

        QVERIFY(!reader.lastFilter.month);
        QVERIFY(!reader.lastFilter.year);
        QVERIFY(reader.lastFilter.paymentType.isEmpty());
    }

    void collectionsTabShowsCollectionColumnsAndSearchesByBuyer()
    {
        CommercialMainView view;
        FakeCommercialReader reader;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        presentation::CommercialPresenter presenter(view, service, runner);
        view.bind(presenter);
        view.show();

        auto *table = view.findChild<QTableWidget *>(QStringLiteral("tableWidget"));
        auto *search = view.findChild<QLineEdit *>(QStringLiteral("searchBox"));
        auto *newSale = view.findChild<QPushButton *>(QStringLiteral("newSaleButton"));
        auto *paymentType = view.findChild<QComboBox *>(QStringLiteral("ComboPaymentType"));
        auto *paymentLabel = view.findChild<QLabel *>(QStringLiteral("label_3"));
        auto *collectionsTab = view.findChild<QPushButton *>(QStringLiteral("collectionsTab"));
        auto *salesTab = view.findChild<QPushButton *>(QStringLiteral("salesTab"));
        QVERIFY(table && search && newSale && paymentType && paymentLabel
                && collectionsTab && salesTab);

        collectionsTab->click();
        QVERIFY(reader.lastFilter.collectionsMode);
        QVERIFY(search->isVisible());
        QVERIFY(newSale->isHidden());
        QVERIFY(paymentType->isHidden());
        QVERIFY(!paymentType->isEnabled());
        QVERIFY(paymentLabel->isHidden());
        QCOMPARE(table->columnCount(), 7);
        QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("Folio"));
        QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("Fecha de Venta"));
        QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Comprador"));
        QCOMPARE(table->horizontalHeaderItem(3)->text(), QStringLiteral("Vehículo (Modelo/Año)"));
        QCOMPARE(table->horizontalHeaderItem(4)->text(), QStringLiteral("Total"));
        QCOMPARE(table->horizontalHeaderItem(5)->text(), QStringLiteral("Pagos"));
        QCOMPARE(table->horizontalHeaderItem(6)->text(), QStringLiteral("Adeudo"));
        QCOMPARE(table->item(0, 4)->data(Qt::UserRole).toDouble(), 250000.0);
        QCOMPARE(table->item(0, 5)->data(Qt::UserRole).toDouble(), 200000.0);
        QCOMPARE(table->item(0, 6)->data(Qt::UserRole).toDouble(), 50000.0);

        int requestedFolio = -1;
        connect(&view, &CommercialMainView::saleDetailsRequested, &view,
                [&requestedFolio](int folio) { requestedFolio = folio; });
        emit table->cellClicked(0, 0);
        QCOMPARE(requestedFolio, 42);

        search->setText(QStringLiteral("Ana"));
        QTRY_COMPARE(reader.lastFilter.searchText, QStringLiteral("Ana"));
        QVERIFY(reader.lastFilter.collectionsMode);

        salesTab->click();
        QVERIFY(!reader.lastFilter.collectionsMode);
        QVERIFY(reader.lastFilter.searchText.isEmpty());
        QVERIFY(search->isHidden());
        QVERIFY(newSale->isVisible());
        QVERIFY(paymentType->isVisible());
        QVERIFY(paymentType->isEnabled());
        QVERIFY(paymentLabel->isVisible());
        QCOMPARE(table->columnCount(), 8);

        view.findChild<QComboBox *>(QStringLiteral("ComboMonth"))->setCurrentIndex(3);
        view.findChild<QComboBox *>(QStringLiteral("ComboYear"))->setCurrentIndex(1);
        paymentType->setCurrentIndex(2);
        QCOMPARE(reader.lastFilter.month, std::optional(3));
        QCOMPARE(reader.lastFilter.year, std::optional(QDate::currentDate().year()));
        QCOMPARE(reader.lastFilter.paymentType, QStringLiteral("Crédito"));
    }

    void salesTabFolioRequestsFullSaleDetails()
    {
        CommercialMainView view;
        FakeCommercialReader reader;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        presentation::CommercialPresenter presenter(view, service, runner);
        view.bind(presenter);
        application::CommercialSaleDto sale;
        sale.folio = 123;
        sale.saleDate = QDate(2026, 6, 12);
        view.showSales({sale});
        auto *table = view.findChild<QTableWidget *>(QStringLiteral("tableWidget"));
        QVERIFY(table);
        QVERIFY(table->verticalHeader()->isHidden());
        QCOMPARE(table->item(0, 0)->foreground().color(), QColor(QStringLiteral("#1769aa")));
        QVERIFY(table->item(0, 0)->font().underline());

        int requestedFolio = -1;
        connect(&view, &CommercialMainView::fullSaleDetailsRequested, &view,
                [&requestedFolio](int folio) { requestedFolio = folio; });
        emit table->cellClicked(0, 0);
        QCOMPARE(requestedFolio, 123);

        auto *collectionsTab = view.findChild<QPushButton *>(QStringLiteral("collectionsTab"));
        QVERIFY(collectionsTab);
        collectionsTab->click();
        connect(&view, &CommercialMainView::saleDetailsRequested, &view,
                [&requestedFolio](int folio) { requestedFolio = folio; });
        requestedFolio = -1;
        emit table->cellClicked(0, 0);
        QCOMPARE(requestedFolio, 123);
    }

    void saleDetailsShowsDatabaseDataAndRestrictsAdminControls()
    {
        application::CommercialSaleDetailsDto details;
        details.folio = 123;
        details.saleDate = QDate(2026, 1, 1);
        details.buyerName = QStringLiteral("Sebastian Hernandez Rojas");
        details.vehicleName = QStringLiteral("Seat Peugeot 208 Allure");
        details.totalAmount = 250000.0;
        details.downPayment = 100000.0;
        details.amountToFinance = 150000.0;
        details.paidAmount = 182500.0;
        details.capitalPaid = 75000.0;
        details.interestPaid = 7500.0;
        details.originalDueAmount = 150000.0;
        details.balanceDue = 67500.0;
        details.paidInstallments = 3;
        details.expectedInstallments = 6;

        application::CommercialPaymentDto payment;
        payment.folio = 1232;
        payment.saleFolio = 123;
        payment.installmentNumber = 2;
        payment.paymentMethod = QStringLiteral("Efectivo");
        payment.scheduledDate = QDate(2026, 2, 1);
        payment.paidDate = QDate(2026, 2, 3);
        payment.paidAmount = 27500.0;
        payment.capitalAllocation = 25000.0;
        payment.interestAllocation = 2500.0;
        payment.remainingBalance = 137500.0;
        payment.waived = true;
        payment.notes = QStringLiteral("Pago recibido");
        details.payments << payment;
        application::CommercialPaymentDto pendingPayment;
        pendingPayment.folio = 1233;
        pendingPayment.saleFolio = 123;
        pendingPayment.installmentNumber = 3;
        pendingPayment.scheduledDate = QDate(2026, 3, 1);
        pendingPayment.expectedAmount = 27500.0;
        details.payments << pendingPayment;

        FakeCommercialReader reader;
        reader.details = details;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        SaleDetails administrator(domain::UserRole::Admin);
        presentation::CommercialSaleDetailsPresenter presenter(
            administrator, service, runner, details.folio, domain::UserRole::Admin);
        administrator.bind(presenter);
        presenter.load();
        auto *adminTable = administrator.findChild<QTableWidget *>(QStringLiteral("paymentsTable"));
        QVERIFY(adminTable);
        QCOMPARE(adminTable->columnCount(), 10);
        QVERIFY(!adminTable->isColumnHidden(9));
        const QStringList paymentHeaders = {
            QStringLiteral("Folio"), QStringLiteral("No"), QStringLiteral("Forma de pago"),
            QStringLiteral("Fecha programada"), QStringLiteral("Fecha de pago"),
            QStringLiteral("Monto"), QStringLiteral("Abono al capital"),
            QStringLiteral("Pago de intereses"), QStringLiteral("Saldo"),
            QStringLiteral("Condonado")
        };
        for (int column = 0; column < paymentHeaders.size(); ++column)
            QCOMPARE(adminTable->horizontalHeaderItem(column)->text(), paymentHeaders.at(column));
        QVERIFY(!administrator.findChild<QPushButton *>(
                     QStringLiteral("updateFinancingButton"))->isHidden());
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("paymentsTitle"))->text(),
                 QStringLiteral("Pagos efectuados (3/6)"));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("vehicleValue"))->text(),
                 QStringLiteral("Seat Peugeot 208 Allure"));
        const QLocale mx(QLocale::Spanish, QLocale::Mexico);
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("totalValue"))->text(),
                 mx.toCurrencyString(details.totalAmount, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("downPaymentValue"))->text(),
                 mx.toCurrencyString(details.downPayment, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("amountToFinanceValue"))->text(),
                 mx.toCurrencyString(details.amountToFinance, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("paidValue"))->text(),
                 mx.toCurrencyString(details.paidAmount, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("capitalPaidValue"))->text(),
                 mx.toCurrencyString(details.capitalPaid, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("interestPaidValue"))->text(),
                 mx.toCurrencyString(details.interestPaid, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("originalDueValue"))->text(),
                 mx.toCurrencyString(details.originalDueAmount, QStringLiteral("$"), 2));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("balanceDueValue"))->text(),
                 mx.toCurrencyString(details.balanceDue, QStringLiteral("$"), 2));
        QCOMPARE(adminTable->item(0, 0)->text(), QStringLiteral("1232"));
        QCOMPARE(adminTable->item(0, 2)->text(), QStringLiteral("Efectivo"));
        QCOMPARE(adminTable->rowCount(), 2);
        auto *waivedContainer = adminTable->cellWidget(0, 9);
        QVERIFY(waivedContainer);
        auto *waived = waivedContainer->findChild<QCheckBox *>();
        QVERIFY(waived && waived->isChecked() && waived->isEnabled());
        auto *waivedLayout = qobject_cast<QHBoxLayout *>(waivedContainer->layout());
        QVERIFY(waivedLayout);
        QCOMPARE(waivedLayout->alignment(), Qt::AlignCenter);

        auto *printButton = administrator.findChild<QPushButton *>(QStringLiteral("printButton"));
        QVERIFY(printButton);
        bool printRequested = false;
        int printedPaymentFolio = -1;
        connect(&administrator, &SaleDetails::paymentPrintRequested, &administrator,
                [&printRequested, &printedPaymentFolio](
                    const application::CommercialSaleDetailsDto &printedSale,
                    const application::CommercialPaymentDto &printedPayment) {
                    printRequested = true;
                    printedPaymentFolio = printedPayment.folio;
                    QCOMPARE(printedSale.folio, 123);
                    QCOMPARE(printedPayment.saleFolio, printedSale.folio);
                });
        adminTable->selectRow(0);
        printButton->click();
        QVERIFY(printRequested);
        QCOMPARE(printedPaymentFolio, 1232);

        printRequested = false;
        adminTable->selectRow(1);
        printButton->click();
        QVERIFY(printRequested);
        QCOMPARE(printedPaymentFolio, 1233);

        waived->click();
        QCOMPARE(reader.waiverUpdates, 1);
        QCOMPARE(reader.lastPaymentFolio, 1232);
        QVERIFY(!reader.lastWaiverValue);

        SaleDetails seller(domain::UserRole::SalesAgent);
        seller.showSaleDetails(details);
        auto *sellerTable = seller.findChild<QTableWidget *>(QStringLiteral("paymentsTable"));
        QVERIFY(sellerTable);
        QVERIFY(sellerTable->isColumnHidden(9));
        QCOMPARE(sellerTable->columnCount(), 10);
        QVERIFY(seller.findChild<QPushButton *>(
                    QStringLiteral("updateFinancingButton"))->isHidden());
    }

    void paymentWaiverRequiresAdministratorRole()
    {
        FakeCommercialReader reader;
        application::CommercialService service(reader, reader);
        const auto denied = service.updatePaymentWaiver(7, true, domain::UserRole::SalesAgent);
        QVERIFY(!denied.ok);
        QCOMPARE(reader.waiverUpdates, 0);

        const auto allowed = service.updatePaymentWaiver(7, true, domain::UserRole::Admin);
        QVERIFY(allowed.ok);
        QCOMPARE(reader.waiverUpdates, 1);
        QCOMPARE(reader.lastPaymentFolio, 7);
        QVERIFY(reader.lastWaiverValue);
    }

    void completeDetailsButtonShowsSavedSaleReadOnly()
    {
        application::CommercialSaleRegistrationDetailsDto saved;
        saved.folio = 456;
        saved.registration.vehicleFolio = 75;
        saved.registration.saleDate = QDate(2026, 8, 14);
        saved.registration.paymentType = domain::PaymentType::Credito;
        saved.registration.subtotal = 200000.0;
        saved.registration.iva = 32000.0;
        saved.registration.totalAmount = 232000.0;
        saved.registration.observations = QStringLiteral("Operación de prueba");
        saved.registration.buyer.fullName = QStringLiteral("María Pérez");
        saved.registration.buyer.nationalId = QStringLiteral("INE-123");
        saved.registration.buyer.streetAddress = QStringLiteral("Calle Norte 12");
        saved.registration.buyer.phone = QStringLiteral("5551234567");
        saved.vehicle.folio = 75;
        saved.vehicle.brandName = QStringLiteral("Nissan");
        saved.vehicle.model = QStringLiteral("Versa");
        saved.vehicle.yearModel = 2024;
        saved.vendor.displayName = QStringLiteral("Vendedor Uno");
        saved.vendor.username = QStringLiteral("vendedor");
        application::SaleFinancingDto financing;
        financing.downPayment = 50000.0;
        financing.amountToFinance = 182000.0;
        financing.interestRatePercent = 8.5;
        financing.monthsTerm = 12;
        financing.finalPrice = 250000.0;
        application::SaleBuyerDto guarantor;
        guarantor.fullName = QStringLiteral("Aval de Prueba");
        guarantor.nationalId = QStringLiteral("AVAL-456");
        financing.guarantor = guarantor;
        application::SaleInstallmentDto installment;
        installment.number = 1;
        installment.dueDate = QDate(2026, 9, 14);
        installment.expectedAmount = 18000.0;
        financing.installments << installment;
        saved.registration.financing = financing;

        FakeCommercialReader reader;
        reader.registrationDetails = saved;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        MainWindow window;
        window.setSaleDetailsFactory([&service, &runner](QWidget *parent, int folio) {
            auto *summary = new SaleDetails(domain::UserRole::SalesAgent, parent);
            auto *presenter = new presentation::CommercialSaleDetailsPresenter(
                *summary, service, runner, folio, domain::UserRole::SalesAgent, summary);
            summary->bind(*presenter);
            presenter->load();
            return summary;
        });
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));
        QVERIFY(stack);
        emit window.commercialView().fullSaleDetailsRequested(saved.folio);

        QTRY_COMPARE(reader.registrationDetailReads, 1);
        auto *fullDetailsPage = stack->currentWidget();
        QCOMPARE(fullDetailsPage->objectName(), QStringLiteral("fullSaleDetailsPage"));
        auto *summary = stack->findChild<SaleDetails *>();
        QVERIFY(summary);
        auto *form = fullDetailsPage->findChild<SaleDetailsView *>(
            QStringLiteral("fullSaleDetailsView"));
        QVERIFY(form);
        QCOMPARE(form->findChild<QLineEdit *>(QStringLiteral("folioEdit"))->text(),
                 QStringLiteral("456"));
        QCOMPARE(form->findChild<QLineEdit *>(QStringLiteral("buyerEdit"))->text(),
                 QStringLiteral("María Pérez"));
        QCOMPARE(form->findChild<QLineEdit *>(QStringLiteral("gNameEdit"))->text(),
                 QStringLiteral("Aval de Prueba"));
        QCOMPARE(form->findChild<QDateEdit *>(QStringLiteral("dateEdit"))->date(),
                 QDate(2026, 8, 14));
        QCOMPARE(form->findChild<QDoubleSpinBox *>(QStringLiteral("totalSpin"))->value(),
                 232000.0);
        QCOMPARE(form->findChild<QTableView *>(QStringLiteral("paymentsTable"))->model()->rowCount(),
                 1);
        QVERIFY(form->findChild<QLineEdit *>(QStringLiteral("buyerEdit"))->isReadOnly());
        QVERIFY(form->findChild<QDateEdit *>(QStringLiteral("dateEdit"))->isReadOnly());
        QVERIFY(form->findChild<QDoubleSpinBox *>(QStringLiteral("totalSpin"))->isReadOnly());
        QVERIFY(!form->findChild<QComboBox *>(QStringLiteral("paymentTypeCombo"))->isEnabled());
        QVERIFY(form->findChild<QPushButton *>(
                     QStringLiteral("calculateProjectionButton"))->isHidden());

        auto *backButton = fullDetailsPage->findChild<QPushButton *>(
            QStringLiteral("backToSaleDetailsButton"));
        QVERIFY(backButton);
        QCOMPARE(backButton->text(), QString());
        QVERIFY(!backButton->icon().isNull());
        QCOMPARE(backButton->minimumSize(), QSize(44, 40));
        backButton->click();
        QTRY_COMPARE(stack->currentWidget(),
                     static_cast<QWidget *>(&window.commercialView()));
        QTRY_VERIFY(!stack->findChild<SaleDetails *>());
    }

    void fullDetailsFromSaleSummaryReturnsToSummary()
    {
        FakeCommercialReader reader;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        MainWindow window;
        window.setSaleDetailsFactory([&service, &runner](QWidget *parent, int folio) {
            auto *summary = new SaleDetails(domain::UserRole::SalesAgent, parent);
            auto *presenter = new presentation::CommercialSaleDetailsPresenter(
                *summary, service, runner, folio, domain::UserRole::SalesAgent, summary);
            summary->bind(*presenter);
            presenter->load();
            return summary;
        });
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));
        QVERIFY(stack);

        emit window.commercialView().saleDetailsRequested(42);
        QTRY_VERIFY(qobject_cast<SaleDetails *>(stack->currentWidget()));
        auto *summary = qobject_cast<SaleDetails *>(stack->currentWidget());
        QVERIFY(summary);
        auto *completeButton =
            summary->findChild<QPushButton *>(QStringLiteral("detailsCompleteButton"));
        QVERIFY(completeButton);
        completeButton->click();

        QTRY_COMPARE(stack->currentWidget()->objectName(),
                     QStringLiteral("fullSaleDetailsPage"));
        auto *backButton = stack->currentWidget()->findChild<QPushButton *>(
            QStringLiteral("backToSaleDetailsButton"));
        QVERIFY(backButton);
        backButton->click();
        QTRY_COMPARE(stack->currentWidget(), static_cast<QWidget *>(summary));
    }

    void collectionFolioOpensSaleDetailsAndBackReturnsToCommercial()
    {
        MainWindow window;
        int openedFolio = -1;
        window.setSaleDetailsFactory([&openedFolio](QWidget *parent, int folio) {
            openedFolio = folio;
            return new SaleDetails(domain::UserRole::Admin, parent);
        });

        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));
        QVERIFY(stack);
        CommercialMainView &commercial = window.commercialView();
        stack->setCurrentWidget(&commercial);

        emit commercial.saleDetailsRequested(42);
        QTRY_VERIFY(stack->currentWidget() != &commercial);
        QCOMPARE(openedFolio, 42);
        auto *details = qobject_cast<SaleDetails *>(stack->currentWidget());
        QVERIFY(details);

        auto *back = details->findChild<QPushButton *>(QStringLiteral("backButton"));
        QVERIFY(back);
        back->click();
        QTRY_COMPARE(stack->currentWidget(), static_cast<QWidget *>(&commercial));
    }
};

QTEST_MAIN(TstCommercialView)
#include "tst_commercialview.moc"
