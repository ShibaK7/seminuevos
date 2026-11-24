#include "application/commercial/services/commercialservice.h"
#include "adapters/contract/promissorynoteprinter.h"
#include "presentation/commercial/commercialmainview.h"
#include "presentation/commercial/commercialpresenter.h"
#include "presentation/commercial/saledetails/SaleDetails.h"
#include "presentation/commercial/saledetails/saledetailspresenter.h"
#include "presentation/commercial/registration/saledetailsview.h"
#include "presentation/commercial/registration/salepaymentconditions.h"
#include "presentation/commercial/registration/salewizardview.h"
#include "presentation/shell/mainwindow.h"
#include "presentation/common/tasks/taskrunner.h"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTableWidget>
#include <QTableView>
#include <QHeaderView>
#include <QStackedWidget>
#include <QTemporaryDir>
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
    int registrationDetailReads = 0;
    int waiverUpdates = 0;
    int lastPaymentFolio = -1;
    bool lastWaiverValue = false;

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
};

} // namespace

class TstCommercialView : public QObject
{
    Q_OBJECT

private slots:
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

    void cancellingANewSaleReturnsToCommercialView()
    {
        MainWindow window;
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));
        auto *newSaleButton = window.findChild<QPushButton *>(QStringLiteral("newSaleButton"));
        auto *cancelButton = window.findChild<QPushButton *>(QStringLiteral("cancelButton"));
        QWidget *saleWizard = window.findChild<QWidget *>(QStringLiteral("commercialPage"));
        QVERIFY(stack && newSaleButton && cancelButton && saleWizard);

        newSaleButton->click();
        QTRY_COMPARE(stack->currentWidget(), saleWizard);

        QTimer::singleShot(0, [] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                auto *dialog = qobject_cast<QMessageBox *>(widget);
                if (!dialog)
                    continue;
                for (QAbstractButton *button : dialog->buttons()) {
                    if (button->text() == QStringLiteral("Sí")) {
                        button->click();
                        return;
                    }
                }
            }
        });
        cancelButton->click();

        QCOMPARE(stack->currentWidget(), &window.commercialView());
    }

    void saleCostsAndFinancingProjectionFollowVehicleType()
    {
        SaleDetailsView form;
        application::SaleVehicleOptionDto acquisition;
        acquisition.folio = 71;
        acquisition.acquisitionType = domain::AcquisitionType::Adquisicion;
        acquisition.salePrice = 200000.0;
        acquisition.maintenanceCost = 1200.0;
        form.setVehicleMatches({acquisition});

        auto *vehicleCombo = form.findChild<QComboBox *>(QStringLiteral("vehicleCombo"));
        auto *ivaCheck = form.findChild<QCheckBox *>(QStringLiteral("ivaCheckBox"));
        auto *maintenanceCheck = form.findChild<QCheckBox *>(QStringLiteral("maintenanceCheckBox"));
        auto *iva = form.findChild<QDoubleSpinBox *>(QStringLiteral("ivaSpin"));
        auto *subtotal = form.findChild<QDoubleSpinBox *>(QStringLiteral("subtotalSpin"));
        auto *total = form.findChild<QDoubleSpinBox *>(QStringLiteral("totalSpin"));
        auto *paymentType = form.findChild<QComboBox *>(QStringLiteral("paymentTypeCombo"));
        auto *deposit = form.findChild<QDoubleSpinBox *>(QStringLiteral("depositSpin"));
        auto *financed = form.findChild<QDoubleSpinBox *>(QStringLiteral("financingSpin"));
        auto *rate = form.findChild<QDoubleSpinBox *>(QStringLiteral("interestRateSpin"));
        auto *months = form.findChild<QSpinBox *>(QStringLiteral("monthsSpin"));
        auto *date = form.findChild<QDateEdit *>(QStringLiteral("dateEdit"));
        auto *depositLabel = form.findChild<QLabel *>(QStringLiteral("depositLabel"));
        auto *table = form.findChild<QTableView *>(QStringLiteral("paymentsTable"));
        auto *printNotesButton = form.findChild<QPushButton *>(QStringLiteral("printButton"));
        QVERIFY(vehicleCombo && ivaCheck && maintenanceCheck && iva && subtotal && total && paymentType
            && deposit && financed && rate && months && date && depositLabel && table
            && printNotesButton);
        QVERIFY(date->calendarPopup());

        vehicleCombo->setCurrentIndex(0);
        QVERIFY(QMetaObject::invokeMethod(vehicleCombo, "activated", Qt::DirectConnection,
                                          Q_ARG(int, 0)));
        QVERIFY(ivaCheck->isChecked());
        QVERIFY(maintenanceCheck->isChecked());
        QCOMPARE(iva->value(), 32000.0);
        QCOMPARE(total->value(), 233200.0);
        QVERIFY(!subtotal->isReadOnly());
        subtotal->setValue(210000.0);
        QCOMPARE(iva->value(), 33600.0);
        QCOMPARE(total->value(), 244800.0);
        subtotal->setValue(200000.0);

        paymentType->setCurrentIndex(
            paymentType->findData(static_cast<int>(domain::PaymentType::Credito)));
        QVERIFY(!printNotesButton->isHidden());
        QVERIFY(!printNotesButton->isEnabled());
        QCOMPARE(rate->value(), 3.0);
        QCOMPARE(deposit->value(), 116600.0);
        QCOMPARE(depositLabel->text(), QStringLiteral("Enganche (50%):"));
        deposit->setValue(100000.0);
        QCOMPARE(depositLabel->text(), QStringLiteral("Enganche (42.88%):"));

        financed->setValue(120000.0);
        months->setValue(12);
        date->setDate(QDate(2026, 6, 15));
        form.findChild<QPushButton *>(QStringLiteral("calculateProjectionButton"))->click();
        QVERIFY(printNotesButton->isEnabled());
        QCOMPARE(table->model()->rowCount(), 12);
        QCOMPARE(table->model()->columnCount(), 8);
        const QLocale locale(QLocale::Spanish, QLocale::Mexico);
        QCOMPARE(table->model()->index(0, 2).data().toString(), locale.toCurrencyString(120000.0));
        QCOMPARE(table->model()->index(0, 3).data().toString(), locale.toCurrencyString(10000.0));
        QCOMPARE(table->model()->index(0, 4).data().toString(), locale.toCurrencyString(3600.0));
        QCOMPARE(table->model()->index(0, 5).data().toString(), locale.toCurrencyString(576.0));
        QCOMPARE(table->model()->index(0, 6).data().toString(), locale.toCurrencyString(14176.0));
        QCOMPARE(table->model()->index(0, 7).data().toString(), locale.toCurrencyString(110000.0));
        QCOMPARE(table->model()->index(1, 4).data().toString(), locale.toCurrencyString(3300.0));
        QCOMPARE(table->model()->index(1, 5).data().toString(), locale.toCurrencyString(528.0));
        QCOMPARE(table->model()->index(1, 6).data().toString(), locale.toCurrencyString(13828.0));
        QCOMPARE(table->model()->index(0, 6).data(Qt::ForegroundRole).value<QBrush>().color(),
                 QColor(QStringLiteral("#1769aa")));
        QVERIFY(table->model()->index(0, 6).data(Qt::FontRole).value<QFont>().bold());

        application::SaleVehicleOptionDto consignment = acquisition;
        consignment.folio = 72;
        consignment.acquisitionType = domain::AcquisitionType::Consignacion;
        form.setVehicleMatches({consignment});
        vehicleCombo->setCurrentIndex(0);
        QVERIFY(QMetaObject::invokeMethod(vehicleCombo, "activated", Qt::DirectConnection,
                                          Q_ARG(int, 0)));
        QVERIFY(!ivaCheck->isChecked());
        QVERIFY(maintenanceCheck->isChecked());
        QCOMPARE(iva->value(), 0.0);
        QCOMPARE(total->value(), 201200.0);
        financed->setValue(120000.0);
        form.findChild<QPushButton *>(QStringLiteral("calculateProjectionButton"))->click();
        QCOMPARE(table->model()->columnCount(), 7);
        QCOMPARE(table->model()->index(0, 5).data().toString(), locale.toCurrencyString(13600.0));
        QCOMPARE(table->model()->index(0, 5).data(Qt::ForegroundRole).value<QBrush>().color(),
                 QColor(QStringLiteral("#1769aa")));
        QVERIFY(printNotesButton->isEnabled());
    }

    void promissoryNoteDocumentContainsOneCompletedNotePerProjection()
    {
        application::SaleRegistrationDto sale;
        sale.saleDate = QDate(2026, 11, 23);
        sale.buyer.fullName = QStringLiteral("María Pérez");
        sale.buyer.streetAddress = QStringLiteral("Calle Norte 12");
        sale.buyer.suburb = QStringLiteral("Centro");
        sale.buyer.locality = QStringLiteral("Martínez de la Torre");
        sale.buyer.state = QStringLiteral("Veracruz");
        sale.buyer.postalCode = QStringLiteral("93600");
        sale.buyer.phone = QStringLiteral("2321234567");

        application::SaleFinancingDto financing;
        financing.monthsTerm = 2;
        application::SaleBuyerDto guarantor;
        guarantor.fullName = QStringLiteral("Juan Aval");
        guarantor.streetAddress = QStringLiteral("Av. Central 45");
        guarantor.suburb = QStringLiteral("Reforma");
        guarantor.locality = QStringLiteral("Martínez de la Torre");
        guarantor.state = QStringLiteral("Veracruz");
        guarantor.postalCode = QStringLiteral("93600");
        guarantor.phone = QStringLiteral("2327654321");
        financing.guarantor = guarantor;

        application::SaleInstallmentDto first;
        first.number = 1;
        first.dueDate = QDate(2026, 12, 23);
        first.expectedAmount = 13125.0;
        financing.installments << first;
        application::SaleInstallmentDto second;
        second.number = 2;
        second.dueDate = QDate(2027, 1, 23);
        second.expectedAmount = 13125.0;
        financing.installments << second;
        sale.financing = financing;

        QString error;
        const QString html = PromissoryNotePrinter::renderHtml(sale, &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!html.isEmpty());
        QCOMPARE(html.count(QStringLiteral("PAGARÉ")), 4); // note + aval heading per page
        QVERIFY(html.contains(QStringLiteral("1/2")));
        QVERIFY(html.contains(QStringLiteral("2/2")));
        QVERIFY(html.contains(QStringLiteral("$13,125.00")));
        QVERIFY(html.contains(QStringLiteral("TRECE MIL CIENTO VEINTICINCO PESOS 00/100 M.N.")));
        QVERIFY(html.contains(QStringLiteral("23/12/2026")));
        QVERIFY(html.contains(QStringLiteral("23/01/2027")));
        QVERIFY(html.contains(QStringLiteral("María Pérez")));
        QVERIFY(html.contains(QStringLiteral("Calle Norte 12, Centro")));
        QVERIFY(html.contains(QStringLiteral("Juan Aval")));
        QVERIFY(html.contains(QStringLiteral("2327654321")));
        QVERIFY(!html.contains(QStringLiteral("{{")));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString pdfPath = tempDir.filePath(QStringLiteral("pagares.pdf"));
        const auto generated = PromissoryNotePrinter().generatePdf(sale, pdfPath);
        QVERIFY2(generated.ok, qPrintable(generated.errorMessage));
        QFile pdf(pdfPath);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(4), QByteArray("%PDF"));
    }

    void creditContractPaymentConditionsSpellOutDeposit()
    {
        application::SaleRegistrationDto sale;
        sale.paymentType = domain::PaymentType::Credito;
        application::SaleFinancingDto financing;
        financing.downPayment = 200000.0;
        financing.amountToFinance = 300000.0;
        financing.monthsTerm = 24;
        financing.interestRatePercent = 3.0;
        sale.financing = financing;

        QCOMPARE(presentation::salePaymentConditions(sale),
                 QStringLiteral("ENGANCHE POR $200,000.00 (DOSCIENTOS MIL PESOS 00/100 M.N.) "
                                "Y EL RESTO A 24 MESES CON UNA TASA DE INTERÉS DEL 3% "
                                "SOBRE SALDOS INSOLUTOS."));
    }

    void cashContractPaymentConditionsUseSelectedMethod()
    {
        application::SaleRegistrationDto sale;
        sale.paymentType = domain::PaymentType::Contado;
        sale.paymentMethod = domain::PaymentMethod::Efectivo;
        QCOMPARE(presentation::salePaymentConditions(sale),
                 QStringLiteral("PAGO DE CONTADO (EFECTIVO) EN UNA SOLA EXHIBICIÓN."));

        sale.paymentMethod = domain::PaymentMethod::Transferencia;
        QCOMPARE(presentation::salePaymentConditions(sale),
                 QStringLiteral("PAGO DE CONTADO (TRANSFERENCIA) EN UNA SOLA EXHIBICIÓN."));
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
        QString paymentMessage;
        QTimer::singleShot(0, [&paymentMessage] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            paymentMessage = dialog->text();
            dialog->accept();
        });
        adminTable->selectRow(0);
        printButton->click();
        QCOMPARE(paymentMessage, QStringLiteral("Folio de pago: 1232"));

        QTimer::singleShot(0, [&paymentMessage] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            paymentMessage = dialog->text();
            dialog->accept();
        });
        adminTable->selectRow(1);
        printButton->click();
        QCOMPARE(paymentMessage, QStringLiteral("No existe una nota para ese pago."));

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
        auto *goBackButton = fullDetailsPage->findChild<QPushButton *>(
            QStringLiteral("goBackButton"));
        QVERIFY(goBackButton && !goBackButton->isHidden());
        QVERIFY(fullDetailsPage->findChild<QPushButton *>(QStringLiteral("cancelButton"))->isHidden());
        QVERIFY(fullDetailsPage->findChild<QPushButton *>(QStringLiteral("saveButton"))->isHidden());
        auto *reprintButton = fullDetailsPage->findChild<QPushButton *>(
            QStringLiteral("printContractButton"));
        QVERIFY(reprintButton && !reprintButton->isHidden() && reprintButton->isEnabled());
        QCOMPARE(reprintButton->text(), QStringLiteral("Reimprimir contrato"));
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

        goBackButton->click();
        QTRY_COMPARE(stack->currentWidget(),
                     static_cast<QWidget *>(&window.commercialView()));
        QTRY_VERIFY(!stack->findChild<SaleDetails *>());
    }

    void fullDetailsFromSaleSummaryReturnsToCommercial()
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
        application::CommercialSaleRegistrationDetailsDto saved;
        saved.folio = 42;
        emit summary->fullSaleDetailsLoaded(saved);

        QTRY_COMPARE(stack->currentWidget()->objectName(),
                     QStringLiteral("fullSaleDetailsPage"));
        auto *goBackButton = stack->currentWidget()->findChild<QPushButton *>(
            QStringLiteral("goBackButton"));
        QVERIFY(goBackButton);
        goBackButton->click();
        QTRY_COMPARE(stack->currentWidget(), static_cast<QWidget *>(&window.commercialView()));
        QTRY_VERIFY(!stack->findChild<SaleDetails *>());
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
