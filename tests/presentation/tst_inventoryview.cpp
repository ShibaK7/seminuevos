// La página del inventario con widgets reales, el presenter y puertos falsos.
// Comprueba que pinte una tarjeta por unidad, el aviso cuando no hay, que un
// cambio de filtro recargue, y que la ventana principal muestre el rol y
// conecte la página con su presenter.

#include "application/inventory/services/inventoryservice.h"
#include "fakes.h"
#include "presentation/inventory/inventorypresenter.h"
#include "presentation/inventory/inventoryview.h"
#include "presentation/shell/mainwindow.h"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
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

        auto *list = window.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        QVERIFY(list);
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
        session.role = QStringLiteral("vendedor");
        window.setSession(session);
        auto *role = window.findChild<QLabel *>(QStringLiteral("userRoleLabel"));
        QVERIFY(role);
        QCOMPARE(role->text(), QStringLiteral("Vendedor"));
    }
};

QTEST_MAIN(TstInventoryView)
#include "tst_inventoryview.moc"
