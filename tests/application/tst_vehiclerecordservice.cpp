// Caso de uso del expediente de un vehículo (US-03.3 y US-03.4) con puertos
// falsos: qué deja ver cada rol y cómo llega lo guardado a la pantalla.

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "fakes.h"
#include "registrationfixtures.h"

#include <QtTest>

using Status = application::RecordLoadResult::Status;

class TstVehicleRecordService : public QObject
{
    Q_OBJECT

private:
    fakes::FakeVehicleRecordReader m_reader;
    fakes::FakeFileStorage m_files;

    application::VehicleRecordService service() { return {m_reader, m_files}; }

private slots:
    void init()
    {
        m_reader = fakes::FakeVehicleRecordReader();
        m_files = fakes::FakeFileStorage();
        m_files.root = QStringLiteral("C:/almacen");
        m_reader.records.insert(7, fixtures::storedRecord(7, domain::AcquisitionType::Adquisicion));
    }

    // La decisión es del dominio (VehicleRecordAccess); el servicio solo la
    // entrega como DTO, que es lo que la pantalla sí puede leer.
    void accessFollowsTheRole()
    {
        const application::RecordAccessDto admin = service().accessFor(domain::UserRole::Admin);
        QVERIFY(admin.canEdit);
        QVERIFY(admin.showsDealTerms);
        QVERIFY(admin.showsMaintenanceHistory);

        const application::RecordAccessDto seller =
            service().accessFor(domain::UserRole::SalesAgent);
        QVERIFY(!seller.canEdit);
        QVERIFY(!seller.showsDealTerms);
        QVERIFY(!seller.showsMaintenanceHistory);
    }

    // La base guarda rutas relativas al almacén; la pantalla las recibe
    // absolutas, con la miniatura de cada foto ya leída.
    void loadResolvesPathsAndThumbnails()
    {
        const application::RecordLoadResult result = service().load(7, domain::UserRole::Admin);
        QCOMPARE(result.status, Status::Loaded);
        const application::VehicleRecordDto &record = result.record;
        QCOMPARE(record.folio, 7);
        QCOMPARE(record.version, QStringLiteral("1001"));

        QCOMPARE(record.images.size(), 2);
        const QString photo = QStringLiteral("C:/almacen/vehicles/VIN1/images/1.jpg");
        QCOMPARE(record.images.at(0).image.path, photo);
        QCOMPARE(record.images.at(0).thumbnail, QByteArray("miniatura:") + photo.toUtf8());
        QCOMPARE(record.documents.first().path,
                 QStringLiteral("C:/almacen/vehicles/VIN1/documents/tarjeta.pdf"));
        QCOMPARE(record.details.invoiceFilePath,
                 QStringLiteral("C:/almacen/vehicles/VIN1/documents/factura.pdf"));
        QCOMPARE(record.conditions.inspection.size(), 1);
    }

    // Exactamente una portada: la guardada; si no hay ninguna, la primera
    // foto (como en la tarjeta del inventario); si hay varias, la primera.
    void loadKeepsExactlyOneCover()
    {
        application::RecordLoadResult result = service().load(7, domain::UserRole::Admin);
        QVERIFY(!result.record.images.at(0).image.isPrimary);
        QVERIFY(result.record.images.at(1).image.isPrimary);

        m_reader.records[7].images[1].image.isPrimary = false;
        result = service().load(7, domain::UserRole::Admin);
        QVERIFY(result.record.images.at(0).image.isPrimary);
        QVERIFY(!result.record.images.at(1).image.isPrimary);

        m_reader.records[7].images[0].image.isPrimary = true;
        m_reader.records[7].images[1].image.isPrimary = true;
        result = service().load(7, domain::UserRole::Admin);
        QVERIFY(result.record.images.at(0).image.isPrimary);
        QVERIFY(!result.record.images.at(1).image.isPrimary);
    }

    void missingFolioIsNotFound()
    {
        const application::RecordLoadResult result = service().load(99, domain::UserRole::Admin);
        QCOMPARE(result.status, Status::NotFound);
    }

    void failedReadIsReported()
    {
        m_reader.failWith = QStringLiteral("server closed the connection");
        const application::RecordLoadResult result = service().load(7, domain::UserRole::Admin);
        QCOMPARE(result.status, Status::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("server closed the connection"));
    }

    // Un Vendedor nunca recibe la operación: ni la contraparte, ni la
    // factura, ni los precios. La tarjeta del vehículo sí.
    void salesAgentNeverGetsTheDeal()
    {
        const application::RecordLoadResult result =
            service().load(7, domain::UserRole::SalesAgent);
        QCOMPARE(result.status, Status::Loaded);
        const application::VehicleDetailsDto &details = result.record.details;
        QVERIFY(details.counterparty.fullName.isEmpty());
        QVERIFY(details.counterparty.nationalId.isEmpty());
        QVERIFY(!details.invoiceType.has_value());
        QVERIFY(details.invoiceNumber.isEmpty());
        QVERIFY(details.invoiceFilePath.isEmpty());
        QCOMPARE(details.purchasePrice, 0.0);
        QCOMPARE(details.salePrice, 0.0);
        QCOMPARE(details.maintenanceCost, 0.0);

        const application::VehicleDetailsDto stored = m_reader.records.value(7).details;
        QCOMPARE(details.serialNumber, stored.serialNumber);
        QCOMPARE(details.model, stored.model);
        QCOMPARE(details.brand.id, stored.brand.id);
        QCOMPARE(details.dealDate, stored.dealDate);
        // Condición y Archivos sí los ve.
        QCOMPARE(result.record.images.size(), 2);
        QCOMPARE(result.record.conditions.inspection.size(), 1);
    }
};

QTEST_APPLESS_MAIN(TstVehicleRecordService)
#include "tst_vehiclerecordservice.moc"
