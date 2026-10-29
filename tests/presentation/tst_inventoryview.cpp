// La página del inventario con widgets reales, el presenter y puertos falsos.
// Comprueba que pinte una tarjeta por unidad, el aviso cuando no hay, que un
// cambio de filtro recargue, que las pestañas filtren por rama, y que la
// ventana principal muestre el rol y conecte la página con su presenter.

#include "application/inventory/services/inventoryservice.h"
#include "fakes.h"
#include "presentation/inventory/inventorypresenter.h"
#include "presentation/inventory/inventoryview.h"
#include "presentation/inventory/record/vehiclerecordview.h"
#include "presentation/inventory/registration/vehiclewizardview.h"
#include "presentation/inventory/vehicleitemlist.h"
#include "presentation/shell/mainwindow.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QtTest>

namespace {

application::InventoryItemDto vehicle(int folio)
{
    application::InventoryItemDto item;
    item.folio = folio;
    item.brandName = QStringLiteral("Nissan");
    item.model = QStringLiteral("Versa");
    item.yearModel = 2020;
    item.status = domain::VehicleStatus::Disponible;
    item.salePrice = 180000.0;
    return item;
}

} // namespace

class TstInventoryView : public QObject
{
    Q_OBJECT

private:
    fakes::FakeInventoryReader m_reader;
    fakes::FakeFileStorage m_files;
    fakes::InlineTaskRunner m_runner;

private slots:
    void init()
    {
        m_reader = fakes::FakeInventoryReader();
    }

    void showsOneCardPerVehicle()
    {
        m_reader.items << vehicle(1) << vehicle(2) << vehicle(3);
        const application::InventoryService service(m_reader, m_files);
        InventoryView window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bind(*presenter);
        presenter->reload();

        auto *list = window.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        QVERIFY(list);
        QCOMPARE(list->count(), 3);
        QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), 3);
    }

    void emptyInventoryShowsTheMessage()
    {
        const application::InventoryService service(m_reader, m_files);
        InventoryView window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bind(*presenter);
        presenter->reload();

        auto *list = window.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        QCOMPARE(list->count(), 1);
        auto *label = qobject_cast<QLabel *>(list->itemWidget(list->item(0)));
        QVERIFY(label);
        QVERIFY(label->text().startsWith(QStringLiteral("No hay vehículos")));
    }

    void changingTheStatusFilterReloads()
    {
        const application::InventoryService service(m_reader, m_files);
        InventoryView window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bind(*presenter);
        presenter->reload();
        QCOMPARE(m_reader.filters.size(), 1);
        QVERIFY(!m_reader.filters.last().status.has_value());

        auto *status = window.findChild<QComboBox *>(QStringLiteral("estado"));
        QVERIFY(status);
        status->setCurrentIndex(1);
        QCOMPARE(m_reader.filters.size(), 2);
        QVERIFY(m_reader.filters.last().status.has_value());
    }

    void switchingTheTabFiltersByBranch()
    {
        const application::InventoryService service(m_reader, m_files);
        InventoryView window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bind(*presenter);
        presenter->reload();
        QCOMPARE(m_reader.filters.size(), 1);
        QCOMPARE(m_reader.filters.last().acquisitionType,
                 std::optional(domain::AcquisitionType::Adquisicion));

        auto *acquisitionTab = window.findChild<QAbstractButton *>(QStringLiteral("acquisitionTab"));
        auto *consignmentTab = window.findChild<QAbstractButton *>(QStringLiteral("consignmentTab"));
        QVERIFY(acquisitionTab && consignmentTab);

        consignmentTab->click();
        QCOMPARE(m_reader.filters.size(), 2);
        QCOMPARE(m_reader.filters.last().acquisitionType,
                 std::optional(domain::AcquisitionType::Consignacion));

        // Volver a la pestaña ya marcada no lee de más.
        consignmentTab->click();
        QCOMPARE(m_reader.filters.size(), 2);

        acquisitionTab->click();
        QCOMPARE(m_reader.filters.size(), 3);
        QCOMPARE(m_reader.filters.last().acquisitionType,
                 std::optional(domain::AcquisitionType::Adquisicion));
    }

    // La página vive en su propio .ui y la ventana la conecta: el presenter
    // pinta en la página que está dentro de la ventana.
    void mainWindowHostsTheInventoryPage()
    {
        m_reader.items << vehicle(5);
        const application::InventoryService service(m_reader, m_files);
        MainWindow window;
        auto *presenter = new presentation::InventoryPresenter(window.inventoryView(), service,
                                                               m_runner, &window);
        window.bindInventory(*presenter);
        presenter->reload();

        auto *inventoryItem = window.findChild<QPushButton *>(QStringLiteral("inventoryItem"));
        auto *contentStack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));
        auto *list = window.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        QVERIFY(inventoryItem && contentStack && list);
        QVERIFY(inventoryItem->isChecked());
        QCOMPARE(contentStack->currentWidget(), &window.inventoryView());
        QCOMPARE(list->count(), 1);
        QCOMPARE(window.inventoryView().objectName(), QStringLiteral("inventoryPage"));
    }

    // El orden de tabulación cruza los dos formularios: después de la lista
    // viene el menú lateral, y después de él las pestañas.
    void tabOrderCrossesBothForms()
    {
        MainWindow window;
        auto *list = window.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        auto *inventoryItem = window.findChild<QWidget *>(QStringLiteral("inventoryItem"));
        auto *reportItem = window.findChild<QWidget *>(QStringLiteral("reportItem"));
        auto *acquisitionTab = window.findChild<QWidget *>(QStringLiteral("acquisitionTab"));
        QVERIFY(list && inventoryItem && reportItem && acquisitionTab);
        // El siguiente que de verdad recibe Tab (la cadena también pasa por
        // widgets internos, como el viewport de la lista).
        const auto nextTabStop = [](QWidget *from) {
            QWidget *next = from->nextInFocusChain();
            while (next != from && !(next->focusPolicy() & Qt::TabFocus))
                next = next->nextInFocusChain();
            return next;
        };
        QCOMPARE(nextTabStop(list), inventoryItem);
        QCOMPARE(nextTabStop(reportItem), acquisitionTab);
    }

    // Responsivo: en una ventana angosta, "Agregar Vehículo" sube a la fila de
    // las pestañas para que los filtros quepan, y regresa cuando hay lugar.
    // Cambiar de fila le cambia el padre, pero no su lugar al tabular.
    void addVehicleButtonMovesUpWhenNarrow()
    {
        MainWindow window;
        auto *button = window.findChild<QWidget *>(QStringLiteral("addVehicleButton"));
        auto *filterBar = window.findChild<QWidget *>(QStringLiteral("filterBar"));
        auto *navBar = window.findChild<QWidget *>(QStringLiteral("navBar"));
        auto *consignmentTab = window.findChild<QWidget *>(QStringLiteral("consignmentTab"));
        QVERIFY(button && filterBar && navBar && consignmentTab);
        const auto nextTabStop = [](QWidget *from) {
            QWidget *next = from->nextInFocusChain();
            while (next != from && !(next->focusPolicy() & Qt::TabFocus))
                next = next->nextInFocusChain();
            return next;
        };

        window.resize(3000, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(button->parentWidget(), filterBar);
        QCOMPARE(nextTabStop(consignmentTab), button);

        // Lo más angosta que deja la ventana: ahí la fila completa ya no cabe.
        window.resize(window.minimumSizeHint().width(), 800);
        QCoreApplication::processEvents();
        QCOMPARE(button->parentWidget(), navBar);
        QVERIFY(button->isVisible());
        QCOMPARE(nextTabStop(consignmentTab), button);

        window.resize(3000, 800);
        QCoreApplication::processEvents();
        QCOMPARE(button->parentWidget(), filterBar);
        QVERIFY(button->isVisible());
        QCOMPARE(nextTabStop(consignmentTab), button);
    }

    // Las tarjetas ocupan el ancho de la lista también después de achicar la
    // ventana: antes se quedaban del ancho anterior y se salían por la
    // derecha.
    void cardsFollowTheListWidth()
    {
        InventoryView page;
        page.showVehicles({vehicle(1), vehicle(2)});
        page.resize(1800, 600);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        auto *list = page.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        QWidget *card = list->itemWidget(list->item(0));
        QVERIFY(card);
        page.resize(page.minimumSizeHint().width(), 600);
        QCoreApplication::processEvents();
        QVERIFY2(card->width() <= list->viewport()->width(),
                 qPrintable(QStringLiteral("tarjeta de %1 px en una lista de %2 px")
                                .arg(card->width())
                                .arg(list->viewport()->width())));
    }

    void showsTheRoleOfTheSession()
    {
        MainWindow window;
        application::SessionDto session;
        session.username = QStringLiteral("ana");
        session.displayName = QStringLiteral("Ana López");
        session.role = domain::UserRole::SalesAgent;
        window.setSession(session);
        auto *role = window.findChild<QLabel *>(QStringLiteral("userRoleLabel"));
        QVERIFY(role);
        QCOMPARE(role->text(), QStringLiteral("Vendedor"));

        session.role = domain::UserRole::Admin;
        window.setSession(session);
        QCOMPARE(role->text(), QStringLiteral("Administrador"));
    }

    // "Detalles" en una tarjeta pide el expediente de su folio.
    void detailsButtonAsksForTheRecord()
    {
        m_reader.items << vehicle(4) << vehicle(9);
        const application::InventoryService service(m_reader, m_files);
        InventoryView window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bind(*presenter);
        presenter->reload();

        QSignalSpy requested(&window, &InventoryView::vehicleDetailsRequested);
        const QList<VehicleItemList *> cards = window.findChildren<VehicleItemList *>();
        QCOMPARE(cards.size(), 2);
        for (VehicleItemList *card : cards)
            card->findChild<QPushButton *>(QStringLiteral("detailsButton"))->click();
        QCOMPARE(requested.size(), 2);
        QList<int> folios{requested.at(0).at(0).toInt(), requested.at(1).at(0).toInt()};
        std::sort(folios.begin(), folios.end());
        QCOMPARE(folios, (QList<int>{4, 9}));
    }

    // La cáscara abre el expediente del folio pedido, lo reutiliza si se pide
    // el mismo y lo reemplaza si se pide otro. La petición llega encolada.
    void mainWindowOpensReusesAndReplacesTheRecord()
    {
        MainWindow window;
        QList<int> created;
        window.setRecordFactory([&created](QWidget *parent, int folio) {
            created << folio;
            return new VehicleRecordView(parent);
        });
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));
        QVERIFY(stack);

        emit window.inventoryView().vehicleDetailsRequested(5);
        QTRY_COMPARE(created, QList<int>{5});
        auto *first = qobject_cast<VehicleRecordView *>(stack->currentWidget());
        QVERIFY(first);

        emit window.inventoryView().vehicleDetailsRequested(5);
        QCoreApplication::processEvents();
        QCOMPARE(created, QList<int>{5});
        QCOMPARE(stack->currentWidget(), first);

        QPointer<VehicleRecordView> firstAlive = first;
        emit window.inventoryView().vehicleDetailsRequested(6);
        QTRY_COMPARE(created, (QList<int>{5, 6}));
        QVERIFY(stack->currentWidget() != first);
        QVERIFY(qobject_cast<VehicleRecordView *>(stack->currentWidget()));
        QTRY_VERIFY(!firstAlive);
        QCOMPARE(stack->count(), 2); // el inventario y el expediente nuevo
    }

    // Un alta que termina en segundo plano no le quita la pantalla al
    // expediente que se está viendo.
    void closingTheWizardDoesNotCoverTheRecord()
    {
        MainWindow window;
        VehicleWizardView *wizard = nullptr;
        window.setWizardFactory([&wizard](QWidget *parent) {
            wizard = new VehicleWizardView(parent);
            return wizard;
        });
        window.setRecordFactory([](QWidget *parent, int) { return new VehicleRecordView(parent); });
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"));

        emit window.inventoryView().addVehicleRequested();
        QVERIFY(wizard);
        QCOMPARE(stack->currentWidget(), wizard);
        emit window.inventoryView().vehicleDetailsRequested(3);
        QTRY_VERIFY(qobject_cast<VehicleRecordView *>(stack->currentWidget()));
        QWidget *record = stack->currentWidget();

        emit wizard->returnToInventory();
        QCOMPARE(stack->currentWidget(), record);
        QCOMPARE(stack->indexOf(wizard), -1);
    }
};

QTEST_MAIN(TstInventoryView)
#include "tst_inventoryview.moc"
