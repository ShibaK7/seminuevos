#include "application/commercial/services/commercialservice.h"
#include "presentation/commercial/commercialmainview.h"
#include "presentation/commercial/commercialpresenter.h"
#include "presentation/commercial/saledetails/SaleDetails.h"
#include "presentation/commercial/saledetails/saledetailspresenter.h"
#include "presentation/shell/mainwindow.h"
#include "presentation/common/tasks/taskrunner.h"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QStackedWidget>
#include <QMessageBox>
#include <QTimer>
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

    void folioCellOpensNoticeWithTheFolioNumber()
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

        QString notice;
        QTimer::singleShot(0, [&notice] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            notice = dialog->text();
            dialog->accept();
        });
        emit table->cellClicked(0, 0);
        QCOMPARE(notice, QStringLiteral("Folio de venta: 123"));
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

        FakeCommercialReader reader;
        reader.details = details;
        application::CommercialService service(reader, reader);
        InlineTaskRunner runner;
        SaleDetails administrator(domain::UserRole::Admin);
        presentation::SaleDetailsPresenter presenter(
            administrator, service, runner, details.folio, domain::UserRole::Admin);
        administrator.bind(presenter);
        presenter.load();
        auto *adminTable = administrator.findChild<QTableWidget *>(QStringLiteral("paymentsTable"));
        QVERIFY(adminTable);
        QCOMPARE(adminTable->columnCount(), 11);
        QVERIFY(!adminTable->isColumnHidden(9));
        const QStringList paymentHeaders = {
            QStringLiteral("Folio"), QStringLiteral("No"), QStringLiteral("Forma de pago"),
            QStringLiteral("Fecha programada"), QStringLiteral("Fecha de pago"),
            QStringLiteral("Monto"), QStringLiteral("Abono al capital"),
            QStringLiteral("Pago de intereses"), QStringLiteral("Saldo"),
            QStringLiteral("Condonado"), QStringLiteral("Nota")
        };
        for (int column = 0; column < paymentHeaders.size(); ++column)
            QCOMPARE(adminTable->horizontalHeaderItem(column)->text(), paymentHeaders.at(column));
        QVERIFY(!administrator.findChild<QPushButton *>(
                     QStringLiteral("updateFinancingButton"))->isHidden());
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("paymentsTitle"))->text(),
                 QStringLiteral("Pagos efectuados (3/6)"));
        QCOMPARE(administrator.findChild<QLabel *>(QStringLiteral("vehicleValue"))->text(),
                 QStringLiteral("Seat Peugeot 208 Allure"));
        QCOMPARE(adminTable->item(0, 0)->text(), QStringLiteral("1232"));
        QCOMPARE(adminTable->item(0, 2)->text(), QStringLiteral("Efectivo"));
        auto *waived = qobject_cast<QCheckBox *>(adminTable->cellWidget(0, 9));
        auto *note = qobject_cast<QCheckBox *>(adminTable->cellWidget(0, 10));
        QVERIFY(waived && waived->isChecked() && waived->isEnabled());
        QVERIFY(note && note->isChecked());
        waived->click();
        QCOMPARE(reader.waiverUpdates, 1);
        QCOMPARE(reader.lastPaymentFolio, 1232);
        QVERIFY(!reader.lastWaiverValue);

        SaleDetails seller(domain::UserRole::SalesAgent);
        seller.showSaleDetails(details);
        auto *sellerTable = seller.findChild<QTableWidget *>(QStringLiteral("paymentsTable"));
        QVERIFY(sellerTable);
        QVERIFY(sellerTable->isColumnHidden(9));
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
