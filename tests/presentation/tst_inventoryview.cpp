// La ventana principal como vista del inventario: con widgets reales, el
// presenter y puertos falsos. Comprueba que pinte una tarjeta por unidad, el
// aviso cuando no hay, que un cambio de filtro recargue y que muestre el rol.

#include "application/services/inventoryservice.h"
#include "fakes.h"
#include "presentation/presenters/inventorypresenter.h"
#include "presentation/views/shell/mainwindow.h"

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
        MainWindow window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bindInventory(*presenter);
        presenter->reload();

        auto *list = window.findChild<QListWidget *>(QStringLiteral("vehicleList"));
        QVERIFY(list);
        QCOMPARE(list->count(), 3);
        QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), 3);
    }

    void emptyInventoryShowsTheMessage()
    {
        const application::InventoryService service(m_reader, m_files);
        MainWindow window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bindInventory(*presenter);
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
        MainWindow window;
        auto *presenter = new presentation::InventoryPresenter(window, service, m_runner, &window);
        window.bindInventory(*presenter);
        presenter->reload();
        QCOMPARE(m_reader.filters.size(), 1);
        QVERIFY(!m_reader.filters.last().status.has_value());

        auto *status = window.findChild<QComboBox *>(QStringLiteral("estado"));
        QVERIFY(status);
        status->setCurrentIndex(1);
        QCOMPARE(m_reader.filters.size(), 2);
        QVERIFY(m_reader.filters.last().status.has_value());
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
