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

    // Un texto que no choca con el de otra corrida ni con el de otro caso.
    static QString uniqueSuffix()
    {
        static int sequence = 0;
        return QStringLiteral("%1%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(++sequence);
    }

    // Un VIN único, anotado para borrar su unidad al terminar (también si la
    // prueba se lo pone a una unidad al corregirla).
    QString uniqueVin()
    {
        const QString vin = QStringLiteral("TST") + uniqueSuffix();
        m_createdSerials << vin;
        return vin;
    }

    // Una foto real (el almacén la recomprime) en la carpeta temporal.
    QString sourcePhoto(const QString &name = QStringLiteral("origen.png"))
    {
        const QString path = m_storage.filePath(name);
        QImage image(40, 30, QImage::Format_RGB32);
        image.fill(Qt::darkBlue);
        image.save(path);
        return path;
    }

    // Registra una unidad y devuelve su folio (o -1, con la prueba ya fallada).
    int registerUnit(const application::VehicleRegistrationDto &dto)
    {
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        PdfContractGenerator contracts;
        const application::VehicleRegistrationService service(vehicles, files, reference, contracts);
        const auto registered = service.registerVehicle(dto);
        if (registered.status != application::RegistrationResult::Status::Registered) {
            qWarning("%s", qPrintable(registered.validation.joinedMessages()
                                      + registered.errorMessage));
            return -1;
        }
        return registered.folio;
    }

    // Lo que la pantalla manda a guardar si nadie cambia nada.
    static application::VehicleUpdateDto unchanged(const application::VehicleRecordDto &record)
    {
        application::VehicleUpdateDto update;
        update.folio = record.folio;
        update.version = record.version;
        update.loadedCounterparty = record.details.counterparty;
        update.counterpartyVersion = record.counterpartyVersion;
        update.details = record.details;
        update.conditions = record.conditions;
        for (const application::StoredImageDto &stored : record.images)
            update.files.images << stored.image;
        update.files.documents = record.documents;
        return update;
    }

    // Corre SQL suelto de la prueba (lo que haría otro módulo, o un dato
    // corrupto).
    bool exec(const QString &sql, int folio)
    {
        ConnectionPool::Handle handle = m_pool->acquire();
        QSqlQuery query(handle.database());
        query.prepare(sql);
        query.bindValue(QStringLiteral(":folio"), folio);
        if (!query.exec()) {
            qWarning("%s", qPrintable(query.lastError().text()));
            return false;
        }
        return true;
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
        const application::VehicleRecordService records(reader, vehicles, files, reference);
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

    // #34: la corrección se guarda en una transacción y se relee igual, en las
    // dos ramas. El costo de mantenimientos que el historial acumuló mientras
    // se capturaba no se pisa, y la foto reemplazada sale del almacén.
    void editsTheRecordInOneTransaction_data()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("compra") << static_cast<int>(domain::AcquisitionType::Adquisicion);
        QTest::newRow("consignación") << static_cast<int>(domain::AcquisitionType::Consignacion);
    }

    void editsTheRecordInOneTransaction()
    {
        QFETCH(int, type);
        const auto branch = static_cast<domain::AcquisitionType>(type);
        const bool isAcquisition = branch == domain::AcquisitionType::Adquisicion;

        application::VehicleRegistrationDto dto = registration(uniqueVin(), branch);
        dto.files.images << domain::VehicleImage{sourcePhoto(), true};
        if (isAcquisition)
            dto.details.invoiceFilePath = dto.files.documents.first().path;
        const int folio = registerUnit(dto);
        QVERIFY(folio > 0);

        SqlVehicleRecordReader reader(*m_pool);
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        const application::VehicleRecordService records(reader, vehicles, files, reference);
        const application::RecordLoadResult loaded = records.load(folio, domain::UserRole::Admin);
        QCOMPARE(loaded.status, application::RecordLoadResult::Status::Loaded);

        application::VehicleUpdateDto update = unchanged(loaded.record);
        update.details.model = QStringLiteral("Corregido");
        update.details.plates = QStringLiteral("COR0001");
        update.details.counterparty.locality = QStringLiteral("Pátzcuaro");
        update.details.observations = QStringLiteral("Corregido en el expediente");
        if (isAcquisition)
            update.details.purchasePrice = 95000.0;
        else
            update.details.commissionRate = 15.0;
        update.conditions.inspection = {{m_lookups.checklist.at(1).id, true, QString()}};
        const QString oldPhoto = loaded.record.images.first().image.path;
        update.files.images = {domain::VehicleImage{sourcePhoto(QStringLiteral("nueva.png")), true}};

        // Mientras se capturaba, el historial le sumó un mantenimiento (#46).
        const QString branchTable = isAcquisition ? QStringLiteral("vehicle_acquisitions")
                                                  : QStringLiteral("vehicle_consignments");
        QVERIFY(exec(QStringLiteral("UPDATE %1 SET maintenance_cost = maintenance_cost + 100 "
                                    "WHERE vehicle_folio = :folio")
                         .arg(branchTable),
                     folio));

        const application::RecordUpdateResult saved = records.update(update, domain::UserRole::Admin);
        QVERIFY2(saved.status == application::RecordUpdateResult::Status::Saved,
                 qPrintable(saved.validation.joinedMessages() + saved.errorMessage));

        const application::RecordLoadResult reread = records.load(folio, domain::UserRole::Admin);
        const application::VehicleRecordDto &record = reread.record;
        QVERIFY(record.version != loaded.record.version);
        QCOMPARE(record.details.model, QStringLiteral("Corregido"));
        QCOMPARE(record.details.plates, QStringLiteral("COR0001"));
        QCOMPARE(record.details.counterparty.locality, QStringLiteral("Pátzcuaro"));
        QCOMPARE(record.details.observations, QStringLiteral("Corregido en el expediente"));
        QCOMPARE(record.details.maintenanceCost, loaded.record.details.maintenanceCost + 100.0);
        if (isAcquisition) {
            QCOMPARE(record.details.purchasePrice, 95000.0);
            // La factura se conservó: misma ruta, mismo archivo.
            QCOMPARE(record.details.invoiceFilePath, loaded.record.details.invoiceFilePath);
            QVERIFY(QFile::exists(record.details.invoiceFilePath));
        } else {
            QCOMPARE(record.details.commissionRate, 15.0);
        }
        QCOMPARE(record.conditions.inspection.size(), 1);
        QVERIFY(record.conditions.inspection.first().isOptimal);
        QCOMPARE(record.documents.size(), 1);
        QCOMPARE(record.documents.first().path, loaded.record.documents.first().path);
        QCOMPARE(record.images.size(), 1);
        QVERIFY(record.images.first().image.path != oldPhoto);
        QVERIFY(QFile::exists(record.images.first().image.path));
        QVERIFY(!QFile::exists(oldPhoto));
    }

    // Una versión vieja no pisa la corrección de otro, y el VIN de otra
    // unidad se rechaza; el propio, aunque cambie, no.
    void staleVersionsAndForeignVinsAreRejected()
    {
        const QString firstVin = uniqueVin();
        const int first = registerUnit(registration(firstVin));
        const int second = registerUnit(registration(uniqueVin()));
        QVERIFY(first > 0 && second > 0);

        SqlVehicleRecordReader reader(*m_pool);
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        const application::VehicleRecordService records(reader, vehicles, files, reference);
        using UpdateStatus = application::RecordUpdateResult::Status;

        const application::VehicleRecordDto read = records.load(first, domain::UserRole::Admin).record;
        application::VehicleUpdateDto update = unchanged(read);
        update.details.model = QStringLiteral("Primera corrección");
        QCOMPARE(records.update(update, domain::UserRole::Admin).status, UpdateStatus::Saved);
        update.details.model = QStringLiteral("Segunda, con la versión vieja");
        QCOMPARE(records.update(update, domain::UserRole::Admin).status, UpdateStatus::Conflict);
        QCOMPARE(records.load(first, domain::UserRole::Admin).record.details.model,
                 QStringLiteral("Primera corrección"));

        application::VehicleUpdateDto other =
            unchanged(records.load(second, domain::UserRole::Admin).record);
        other.details.serialNumber = firstVin;
        const application::RecordUpdateResult duplicate =
            records.update(other, domain::UserRole::Admin);
        QCOMPARE(duplicate.status, UpdateStatus::Rejected);
        QVERIFY(duplicate.validation.fields().contains(QStringLiteral("serialNumber")));

        other.details.serialNumber = uniqueVin();
        QCOMPARE(records.update(other, domain::UserRole::Admin).status, UpdateStatus::Saved);
    }

    // La contraparte: con la misma identificación se corrige donde está (y se
    // ve en todas sus operaciones); con otra, la operación pasa a quien la
    // tiene o a una persona nueva; y si es de alguien con otro nombre, se
    // rechaza en vez de ligarla en silencio.
    void counterpartyIsCorrectedOrRelinked()
    {
        const QString sharedId = QStringLiteral("ID-COMPARTIDA-") + uniqueSuffix();
        application::VehicleRegistrationDto dto = registration(uniqueVin());
        dto.details.counterparty.fullName = QStringLiteral("Ana Prueba");
        dto.details.counterparty.nationalId = sharedId;
        const int first = registerUnit(dto);
        dto.details.serialNumber = uniqueVin();
        const int second = registerUnit(dto);
        QVERIFY(first > 0 && second > 0);

        SqlVehicleRecordReader reader(*m_pool);
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        const application::VehicleRecordService records(reader, vehicles, files, reference);
        using UpdateStatus = application::RecordUpdateResult::Status;
        const auto counterpartyOf = [&records](int folio) {
            return records.load(folio, domain::UserRole::Admin).record.details.counterparty;
        };
        const auto edit = [&records](int folio, const QString &name, const QString &nationalId) {
            application::VehicleUpdateDto update =
                unchanged(records.load(folio, domain::UserRole::Admin).record);
            update.details.counterparty.fullName = name;
            update.details.counterparty.nationalId = nationalId;
            return records.update(update, domain::UserRole::Admin);
        };

        // Misma identificación: se corrige el nombre en las dos operaciones.
        QCOMPARE(edit(first, QStringLiteral("Ana María Prueba"), sharedId).status,
                 UpdateStatus::Saved);
        QCOMPARE(counterpartyOf(second).fullName, QStringLiteral("Ana María Prueba"));

        // Otra identificación, nueva: la primera pasa a otra persona y la
        // segunda se queda con la de antes.
        const QString newId = QStringLiteral("ID-NUEVA-") + uniqueSuffix();
        QCOMPARE(edit(first, QStringLiteral("Beto Prueba"), newId).status, UpdateStatus::Saved);
        QCOMPARE(counterpartyOf(first).nationalId, newId);
        QCOMPARE(counterpartyOf(first).fullName, QStringLiteral("Beto Prueba"));
        QCOMPARE(counterpartyOf(second).fullName, QStringLiteral("Ana María Prueba"));

        // Una identificación que ya es de otra persona, con otro nombre: no.
        const application::RecordUpdateResult taken =
            edit(first, QStringLiteral("Carlos Prueba"), sharedId);
        QCOMPARE(taken.status, UpdateStatus::Rejected);
        QVERIFY(taken.validation.fields().contains(QStringLiteral("counterparty.nationalId")));
        QVERIFY(taken.validation.joinedMessages().contains(QStringLiteral("Ana María Prueba")));
        QCOMPARE(counterpartyOf(first).nationalId, newId);

        // Con el mismo nombre (sin importar mayúsculas), se liga sin tocarla.
        QCOMPARE(edit(first, QStringLiteral("ana maría prueba"), sharedId).status,
                 UpdateStatus::Saved);
        QCOMPARE(counterpartyOf(first).nationalId, sharedId);
        QCOMPARE(counterpartyOf(first).fullName, QStringLiteral("Ana María Prueba"));
    }

    // Dos administradores con dos unidades de la misma persona. Lo que uno
    // corrige de ella no lo deshace el otro con lo que leyó antes: si el otro
    // no tocó la contraparte, se guarda sin escribirla; si la cambió, es un
    // conflicto.
    void staleFormsDoNotUndoACounterpartyCorrection()
    {
        const QString sharedId = QStringLiteral("ID-COMPARTIDA-") + uniqueSuffix();
        application::VehicleRegistrationDto dto = registration(uniqueVin());
        dto.details.counterparty.fullName = QStringLiteral("Paula Prueba");
        dto.details.counterparty.nationalId = sharedId;
        dto.details.counterparty.streetAddress = QStringLiteral("Calle 1");
        const int first = registerUnit(dto);
        dto.details.serialNumber = uniqueVin();
        const int second = registerUnit(dto);
        QVERIFY(first > 0 && second > 0);

        SqlVehicleRecordReader reader(*m_pool);
        SqlVehicleRepository vehicles(*m_pool);
        SqlReferenceDataReader reference(*m_pool);
        LocalFileStorage files(m_storage.path());
        const application::VehicleRecordService records(reader, vehicles, files, reference);
        using UpdateStatus = application::RecordUpdateResult::Status;

        // B abre la segunda unidad; A corrige la calle desde la primera.
        const application::VehicleRecordDto staleSecond =
            records.load(second, domain::UserRole::Admin).record;
        application::VehicleUpdateDto fromFirst =
            unchanged(records.load(first, domain::UserRole::Admin).record);
        fromFirst.details.counterparty.streetAddress = QStringLiteral("Calle 2");
        QCOMPARE(records.update(fromFirst, domain::UserRole::Admin).status, UpdateStatus::Saved);

        // B cambia la colonia con lo que leyó antes: conflicto, nada se guarda.
        application::VehicleUpdateDto staleCounterparty = unchanged(staleSecond);
        staleCounterparty.details.counterparty.suburb = QStringLiteral("Centro");
        QCOMPARE(records.update(staleCounterparty, domain::UserRole::Admin).status,
                 UpdateStatus::Conflict);

        // B cambia solo el kilometraje: se guarda, y la calle sigue corregida.
        application::VehicleUpdateDto mileageOnly = unchanged(staleSecond);
        mileageOnly.details.mileage = 61000;
        QCOMPARE(records.update(mileageOnly, domain::UserRole::Admin).status, UpdateStatus::Saved);
        const application::VehicleRecordDto reread =
            records.load(second, domain::UserRole::Admin).record;
        QCOMPARE(reread.details.mileage, 61000);
        QCOMPARE(reread.details.counterparty.streetAddress, QStringLiteral("Calle 2"));
        QVERIFY(reread.details.counterparty.suburb.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstRegistrationDb)
#include "tst_registration_db.moc"
