// Pruebas del presenter del inventario: avisos distintos para "falló" y "no
// hay", portadas leídas por el servicio, desplazamiento a la unidad nueva y
// respuestas viejas descartadas.

#include "application/services/inventoryservice.h"
#include "fakes.h"
#include "presentation/presenters/inventorypresenter.h"

#include <QtTest>

namespace {

application::InventoryItemDto item(int folio, const QString &cover = QString())
{
    application::InventoryItemDto dto;
    dto.folio = folio;
    dto.model = QStringLiteral("Versa");
    dto.coverImagePath = cover;
    return dto;
}

} // namespace

class TstInventoryPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::FakeInventoryReader m_reader;
    fakes::FakeFileStorage m_files;
    fakes::FakeInventoryView m_view;

private slots:
    void init()
    {
        m_reader = fakes::FakeInventoryReader();
        m_files = fakes::FakeFileStorage();
        m_view = fakes::FakeInventoryView();
    }

    void showsVehiclesWithTheirCover()
    {
        m_reader.items << item(1, QStringLiteral("vehicles/VIN/images/a.jpg")) << item(2);
        const application::InventoryService service(m_reader, m_files);
        fakes::InlineTaskRunner runner;
        presentation::InventoryPresenter presenter(m_view, service, runner);

        presenter.reload();
        QCOMPARE(m_view.shown.size(), 2);
        // FakeFileStorage::read devuelve la ruta como bytes.
        QCOMPARE(m_view.shown.at(0).coverImage, QByteArray("vehicles/VIN/images/a.jpg"));
        QVERIFY(m_view.shown.at(1).coverImage.isEmpty());
        QVERIFY(m_view.messages.isEmpty());
    }

    void failureAndEmptyAreDifferentMessages()
    {
        const application::InventoryService service(m_reader, m_files);
        fakes::InlineTaskRunner runner;
        presentation::InventoryPresenter presenter(m_view, service, runner);

        presenter.reload();
        QCOMPARE(m_view.messages.size(), 1);
        QVERIFY(m_view.messages.last().startsWith(QStringLiteral("No hay vehículos")));

        m_reader.failWith = QStringLiteral("server closed the connection");
        presenter.reload();
        QVERIFY(m_view.messages.last().startsWith(QStringLiteral("No se pudo leer el inventario")));
        QVERIFY(m_view.messages.last().contains(QStringLiteral("server closed")));
    }

    void passesTheViewFilter()
    {
        m_view.currentFilter.status = domain::VehicleStatus::Vendido;
        const application::InventoryService service(m_reader, m_files);
        fakes::InlineTaskRunner runner;
        presentation::InventoryPresenter presenter(m_view, service, runner);
        presenter.reload();
        QCOMPARE(m_reader.filters.size(), 1);
        QCOMPARE(m_reader.filters.first().status, std::optional(domain::VehicleStatus::Vendido));
    }

    void registeredVehicleIsScrolledTo()
    {
        m_reader.items << item(7);
        const application::InventoryService service(m_reader, m_files);
        fakes::InlineTaskRunner runner;
        presentation::InventoryPresenter presenter(m_view, service, runner);
        presenter.vehicleRegistered(7);
        QCOMPARE(m_view.scrolledTo, QList<int>{7});
        // Solo una vez: una recarga posterior no vuelve a desplazar.
        presenter.reload();
        QCOMPARE(m_view.scrolledTo.size(), 1);
    }

    // El usuario cambia el filtro dos veces seguidas y la primera lectura
    // llega al final: no debe pisar a la segunda.
    void staleResponsesAreDiscarded()
    {
        const application::InventoryService service(m_reader, m_files);
        fakes::DeferredTaskRunner runner;
        presentation::InventoryPresenter presenter(m_view, service, runner);

        m_reader.items = {item(1)};
        presenter.reload();
        presenter.reload();
        QCOMPARE(runner.works.size(), 2);

        runner.finish(1);
        QCOMPARE(m_view.shown.size(), 1);
        m_reader.items = {item(1), item(2), item(3)};
        runner.finish(0);
        QCOMPARE(m_view.shown.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TstInventoryPresenter)
#include "tst_inventorypresenter.moc"
