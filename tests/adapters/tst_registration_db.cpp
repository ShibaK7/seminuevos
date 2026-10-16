// Prueba de integración del registro y del expediente contra el PostgreSQL de
// Docker, con los adaptadores reales (SqlVehicleRepository,
// SqlVehicleRecordReader, SqlReferenceDataReader y LocalFileStorage sobre una
// carpeta temporal). Es opcional: solo se compila con -DSEMINUEVOS_DB_TESTS=ON
// y necesita `docker compose up -d`.
//
// Cada caso registra una unidad con un VIN único y la borra al terminar.

#include "adapters/contract/pdfcontractgenerator.h"
#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/inventory/sqlinventoryreader.h"
#include "adapters/persistence/inventory/sqlreferencedatareader.h"
#include "adapters/persistence/inventory/sqlvehiclerecordreader.h"
#include "adapters/persistence/inventory/sqlvehiclerepository.h"
#include "adapters/storage/localfilestorage.h"
#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include <memory>

namespace {

QMap<QString, QString> readEnv()
{
    QMap<QString, QString> values;
    QFile file(QStringLiteral(SEMINUEVOS_SOURCE_DIR) + QStringLiteral("/.env"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            const int eq = line.indexOf(QLatin1Char('='));
            if (!line.startsWith(QLatin1Char('#')) && eq > 0)
                values.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
        }
    }
    return values;
}

} // namespace

class TstRegistrationDb : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_storage;
    std::unique_ptr<ConnectionPool> m_pool;
    QStringList m_createdSerials;

    application::RegistrationLookupsDto m_lookups;

    // Un VIN que no choca con el de otra corrida ni con el de otro caso.
    QString uniqueVin()
    {
        static int sequence = 0;
        const QString vin = QStringLiteral("TST%1%2")
                                .arg(QDateTime::currentMSecsSinceEpoch())
                                .arg(++sequence);
        m_createdSerials << vin;
        return vin;
    }

    // Una foto real (el almacén la recomprime) en la carpeta temporal.
    QString sourcePhoto()
    {
        const QString path = m_storage.filePath(QStringLiteral("origen.png"));
        QImage image(40, 30, QImage::Format_RGB32);
        image.fill(Qt::darkBlue);
        image.save(path);
        return path;
    }

    application::VehicleRegistrationDto registration(
        const QString &vin, domain::AcquisitionType type = domain::AcquisitionType::Adquisicion)
    {
        // Un archivo real para que el almacén tenga qué copiar.
        const QString source = m_storage.filePath(QStringLiteral("origen.pdf"));
        QFile file(source);
        if (file.open(QIODevice::WriteOnly))
            file.write("%PDF-1.4 prueba");
        file.close();

        application::VehicleRegistrationDto dto;
        auto &d = dto.details;
        d.acquisitionType = type;
        d.dealDate = QDate::currentDate();
        // Ids reales de los catálogos sembrados por init-db.
        const application::CatalogOptionDto subtype = m_lookups.vehicleSubtypes.first();
        d.vehicleType = {subtype.parentId, QStringLiteral("Tipo")};
        d.subtype = {subtype.id, subtype.name};
        d.brand = {m_lookups.brands.first().id, m_lookups.brands.first().name};
        d.model = QStringLiteral("Prueba integración");
        d.yearModel = 2020;
        d.motorNumber = QStringLiteral("MOTOR1");
        d.serialNumber = vin;
        d.repuve = QStringLiteral("REPUVE1");
        d.plates = QStringLiteral("PRB0001");
        d.platesHolder = QStringLiteral("Titular de prueba");
        d.counterparty.fullName = QStringLiteral("Vendedor de prueba");
        d.counterparty.nationalId = QStringLiteral("ID-") + vin;
        d.invoiceNumber = QStringLiteral("F-PRUEBA");
        if (type == domain::AcquisitionType::Adquisicion) {
            d.invoiceType = domain::InvoiceType::Facturado;
            d.purchasePrice = 100000.0;
            d.salePrice = 120000.0;
            d.paymentMethod = domain::PaymentMethod::Transferencia;
        } else {
            d.invoiceType = domain::InvoiceType::FacturadoReal;
            d.basePrice = 200000.0;
            d.commissionRate = 12.5;
        }

        auto &c = dto.conditions;
        c.fuelType = {m_lookups.fuelTypes.first().id, m_lookups.fuelTypes.first().name};
        c.cylinders = 4;
        c.transmission = domain::Transmission::Manual;
        c.interiorMaterial = QStringLiteral("Tela");
        c.windowRegulators = domain::WindowRegulators::Manuales;
        c.airConditioning = domain::AirConditioning::Manual;

        dto.files.documents << domain::VehicleDocument{QStringLiteral("Tarjeta de circulación"),
                                                       source, QString()};
        return dto;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_storage.isValid());
        const QMap<QString, QString> env = readEnv();
        DatabaseConfig config;
        config.hostName = env.value(QStringLiteral("POSTGRES_HOST"), QStringLiteral("localhost"));
        config.port = env.value(QStringLiteral("POSTGRES_PORT"), QStringLiteral("5432")).toInt();
        config.databaseName = env.value(QStringLiteral("POSTGRES_DB"), QStringLiteral("seminuevos"));
        config.userName = env.value(QStringLiteral("POSTGRES_USER"), QStringLiteral("seminuevos_app"));
        config.password = env.value(QStringLiteral("POSTGRES_PASSWORD"));
        m_pool = std::make_unique<ConnectionPool>(config);
        // Las pruebas corren en el hilo principal.
        m_pool->setGuiThreadAllowed(true);

        {
            ConnectionPool::Handle handle = m_pool->acquire();
            if (!handle.database().isOpen())
                QSKIP("PostgreSQL no está disponible (docker compose up -d).");
        }

        SqlReferenceDataReader reference(*m_pool);
        QString error;
        m_lookups.vehicleSubtypes.clear();
        for (const auto &option : reference.vehicleCategories(&error)) {
            if (option.parentId > 0)
                m_lookups.vehicleSubtypes << option;
        }
        m_lookups.brands = reference.brands(&error);
        m_lookups.fuelTypes = reference.fuelTypes(&error);
        m_lookups.checklist = reference.conditionChecklist(&error);
        QVERIFY2(!m_lookups.vehicleSubtypes.isEmpty() && !m_lookups.brands.isEmpty()
                     && !m_lookups.fuelTypes.isEmpty() && m_lookups.checklist.size() >= 2,
                 qPrintable(error));
    }

    void cleanupTestCase()
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        for (const QString &vin : std::as_const(m_createdSerials)) {
            QSqlQuery query(handle.database());
            query.prepare(QStringLiteral("DELETE FROM vehicles WHERE serial_number = :vin"));
            query.bindValue(QStringLiteral(":vin"), vin);
            query.exec();
        }
    }

    void registersAgainstTheRealDatabase()
    {
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        PdfContractGenerator contracts;
        const application::VehicleRegistrationService service(vehicles, files, reference, contracts);

        const QString vin = uniqueVin();

        const auto first = service.registerVehicle(registration(vin));
        QVERIFY2(first.status == application::RegistrationResult::Status::Registered,
                 qPrintable(first.validation.joinedMessages() + first.errorMessage));
        QCOMPARE(first.status, application::RegistrationResult::Status::Registered);
        QVERIFY(first.folio > 0);
        QVERIFY(first.contract.has_value());

        // La rejilla del inventario la encuentra, con su precio de venta.
        SqlInventoryReader inventory(*m_pool);
        QString inventoryError;
        const auto items = inventory.search(application::InventoryFilterDto(), &inventoryError);
        QVERIFY2(inventoryError.isEmpty(), qPrintable(inventoryError));
        const auto found = std::find_if(items.cbegin(), items.cend(),
                                        [&](const auto &item) { return item.folio == first.folio; });
        QVERIFY(found != items.cend());
        QCOMPARE(found->salePrice, std::optional<double>(120000.0));

        // El mismo VIN otra vez: lo detecta antes de copiar.
        const auto second = service.registerVehicle(registration(vin));
        QCOMPARE(second.status, application::RegistrationResult::Status::Rejected);
        QVERIFY(second.validation.fields().contains(QStringLiteral("serialNumber")));
    }

    void lookupsComeFromTheCatalogs()
    {
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        PdfContractGenerator contracts;
        const application::VehicleRegistrationService service(vehicles, files, reference, contracts);

        const auto lookups = service.loadLookups();
        QVERIFY2(lookups.errorMessage.isEmpty(), qPrintable(lookups.errorMessage));
        QVERIFY(!lookups.vehicleTypes.isEmpty());
        QVERIFY(!lookups.vehicleSubtypes.isEmpty());
        QVERIFY(!lookups.brands.isEmpty());
        QVERIFY(!lookups.checklist.isEmpty());
        QVERIFY(lookups.umaDailyValue.has_value());
    }

    // #30: el expediente se relee completo, con los nombres de sus catálogos y
    // las rutas relativas que guarda la base, en las dos ramas.
    void readsTheRecordBack_data()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("compra") << static_cast<int>(domain::AcquisitionType::Adquisicion);
        QTest::newRow("consignación") << static_cast<int>(domain::AcquisitionType::Consignacion);
    }

    void readsTheRecordBack()
    {
        QFETCH(int, type);
        const auto branch = static_cast<domain::AcquisitionType>(type);
        const bool isAcquisition = branch == domain::AcquisitionType::Adquisicion;

        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        PdfContractGenerator contracts;
        const application::VehicleRegistrationService service(vehicles, files, reference, contracts);

        application::VehicleRegistrationDto dto = registration(uniqueVin(), branch);
        dto.files.images << domain::VehicleImage{sourcePhoto(), true};
        if (isAcquisition)
            dto.details.invoiceFilePath = dto.files.documents.first().path;
        dto.details.color = QStringLiteral("Rojo");
        dto.details.counterparty.locality = QStringLiteral("Morelia");
        dto.details.observations = QStringLiteral("Sin detalles");
        const int faulty = m_lookups.checklist.at(1).id;
        dto.conditions.inspection = {{m_lookups.checklist.at(0).id, true, QString()},
                                     {faulty, false, QStringLiteral("Rayón")}};
        const auto registered = service.registerVehicle(dto);
        QVERIFY2(registered.status == application::RegistrationResult::Status::Registered,
                 qPrintable(registered.validation.joinedMessages() + registered.errorMessage));

        SqlVehicleRecordReader reader(*m_pool);
        QString error;
        const std::optional<application::VehicleRecordDto> record =
            reader.findByFolio(registered.folio, &error);
        QVERIFY2(record.has_value(), qPrintable(error));
        QCOMPARE(record->folio, registered.folio);
        QVERIFY(!record->version.isEmpty());
        QVERIFY(record->hasDealTerms);

        const application::VehicleDetailsDto &d = record->details;
        const application::VehicleDetailsDto &expected = dto.details;
        QCOMPARE(d.acquisitionType, branch);
        QCOMPARE(d.dealDate, expected.dealDate);
        QCOMPARE(d.serialNumber, expected.serialNumber);
        QCOMPARE(d.model, expected.model);
        QCOMPARE(d.yearModel, expected.yearModel);
        QCOMPARE(d.color, expected.color);
        QCOMPARE(d.plates, expected.plates);
        // Los nombres salen de los catálogos, no de lo que se capturó.
        QCOMPARE(d.subtype.id, expected.subtype.id);
        QCOMPARE(d.subtype.name, expected.subtype.name);
        QCOMPARE(d.vehicleType.id, expected.vehicleType.id);
        QVERIFY(!d.vehicleType.name.isEmpty());
        QCOMPARE(d.brand.name, expected.brand.name);
        QCOMPARE(d.counterparty.fullName, expected.counterparty.fullName);
        QCOMPARE(d.counterparty.nationalId, expected.counterparty.nationalId);
        QCOMPARE(d.counterparty.locality, expected.counterparty.locality);
        QCOMPARE(d.invoiceType, expected.invoiceType);
        QCOMPARE(d.invoiceNumber, expected.invoiceNumber);
        QCOMPARE(d.observations, expected.observations);
        if (isAcquisition) {
            QCOMPARE(d.purchasePrice, expected.purchasePrice);
            QCOMPARE(d.salePrice, expected.salePrice);
            QCOMPARE(d.paymentMethod, expected.paymentMethod);
            QVERIFY(!d.invoiceFilePath.isEmpty());
            QVERIFY(QDir::isRelativePath(d.invoiceFilePath));
        } else {
            QCOMPARE(d.basePrice, expected.basePrice);
            QCOMPARE(d.commissionRate, expected.commissionRate);
            QVERIFY(d.invoiceFilePath.isEmpty());
        }

        const application::VehicleConditionsDto &c = record->conditions;
        QCOMPARE(c.fuelType.id, dto.conditions.fuelType.id);
        QCOMPARE(c.fuelType.name, dto.conditions.fuelType.name);
        QCOMPARE(c.cylinders, dto.conditions.cylinders);
        QCOMPARE(c.transmission, dto.conditions.transmission);
        QCOMPARE(c.interiorMaterial, dto.conditions.interiorMaterial);
        QCOMPARE(c.windowRegulators, dto.conditions.windowRegulators);
        QCOMPARE(c.airConditioning, dto.conditions.airConditioning);
        QCOMPARE(c.inspection.size(), 2);
        for (const application::InspectionEntryDto &entry : c.inspection) {
            QCOMPARE(entry.isOptimal, entry.elementId != faulty);
            if (entry.elementId == faulty)
                QCOMPARE(entry.observations, QStringLiteral("Rayón"));
        }

        QCOMPARE(record->images.size(), 1);
        QVERIFY(record->images.first().image.isPrimary);
        QVERIFY(QDir::isRelativePath(record->images.first().image.path));
        QCOMPARE(record->documents.size(), 1);
        QCOMPARE(record->documents.first().documentType, QStringLiteral("Tarjeta de circulación"));
        QVERIFY(QDir::isRelativePath(record->documents.first().path));

        // El servicio las entrega absolutas: son archivos del almacén.
        const application::VehicleRecordService records(reader, files);
        const application::RecordLoadResult loaded =
            records.load(registered.folio, domain::UserRole::Admin);
        QCOMPARE(loaded.status, application::RecordLoadResult::Status::Loaded);
        QVERIFY(QFile::exists(loaded.record.images.first().image.path));
        QVERIFY(!loaded.record.images.first().thumbnail.isEmpty());
        QVERIFY(QFile::exists(loaded.record.documents.first().path));
        if (isAcquisition)
            QVERIFY(QFile::exists(loaded.record.details.invoiceFilePath));
    }

    // Solo cuenta la subtabla de la rama de la unidad, aunque por un dato
    // corrupto exista también la otra; y sin la suya, la unidad se lee igual,
    // sin operación y con la fecha en que se agregó.
    void onlyTheUnitsBranchCounts()
    {
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        PdfContractGenerator contracts;
        const application::VehicleRegistrationService service(vehicles, files, reference, contracts);
        const auto registered = service.registerVehicle(registration(uniqueVin()));
        QVERIFY2(registered.status == application::RegistrationResult::Status::Registered,
                 qPrintable(registered.validation.joinedMessages() + registered.errorMessage));

        {
            ConnectionPool::Handle handle = m_pool->acquire();
            QSqlQuery query(handle.database());
            query.prepare(QStringLiteral(
                "INSERT INTO vehicle_consignments (vehicle_folio, owner_id, invoice_type, "
                "base_price, commission_rate) "
                "SELECT vehicle_folio, seller_id, 'No Facturado', 999, 50 "
                "FROM vehicle_acquisitions WHERE vehicle_folio = :folio"));
            query.bindValue(QStringLiteral(":folio"), registered.folio);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        }

        SqlVehicleRecordReader reader(*m_pool);
        QString error;
        std::optional<application::VehicleRecordDto> record =
            reader.findByFolio(registered.folio, &error);
        QVERIFY2(record.has_value(), qPrintable(error));
        QCOMPARE(record->details.purchasePrice, 100000.0);
        QCOMPARE(record->details.basePrice, 0.0);
        QCOMPARE(record->details.invoiceType, std::optional(domain::InvoiceType::Facturado));

        {
            ConnectionPool::Handle handle = m_pool->acquire();
            QSqlQuery query(handle.database());
            query.prepare(QStringLiteral(
                "DELETE FROM vehicle_acquisitions WHERE vehicle_folio = :folio"));
            query.bindValue(QStringLiteral(":folio"), registered.folio);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        }
        record = reader.findByFolio(registered.folio, &error);
        QVERIFY2(record.has_value(), qPrintable(error));
        QVERIFY(!record->hasDealTerms);
        QVERIFY(record->details.counterparty.fullName.isEmpty());
        QCOMPARE(record->details.dealDate, QDate::currentDate());
    }

    void missingFolioReadsAsNotFound()
    {
        SqlVehicleRecordReader reader(*m_pool);
        QString error;
        QVERIFY(!reader.findByFolio(-1, &error).has_value());
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TstRegistrationDb)
#include "tst_registration_db.moc"
