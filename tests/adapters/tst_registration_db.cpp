// Prueba de integración del registro contra el PostgreSQL de Docker, con los
// adaptadores reales (SqlVehicleRepository, SqlReferenceDataReader y
// LocalFileStorage sobre una carpeta temporal). Es opcional: solo se compila
// con -DSEMINUEVOS_DB_TESTS=ON y necesita `docker compose up -d`.
//
// Cada caso registra una unidad con un VIN único y la borra al terminar.

#include "adapters/contract/pdfcontractgenerator.h"
#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/sqlreferencedatareader.h"
#include "adapters/persistence/sqlvehiclerepository.h"
#include "adapters/storage/localfilestorage.h"
#include "application/services/vehicleregistrationservice.h"

#include <QDateTime>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

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
    QStringList m_createdSerials;

    application::RegistrationLookupsDto m_lookups;

    application::VehicleRegistrationDto registration(const QString &vin)
    {
        // Un archivo real para que el almacén tenga qué copiar.
        const QString source = m_storage.filePath(QStringLiteral("origen.pdf"));
        QFile file(source);
        if (file.open(QIODevice::WriteOnly))
            file.write("%PDF-1.4 prueba");
        file.close();

        application::VehicleRegistrationDto dto;
        auto &d = dto.details;
        d.acquisitionType = domain::AcquisitionType::Adquisicion;
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
        d.invoiceType = domain::InvoiceType::Facturado;
        d.invoiceNumber = QStringLiteral("F-PRUEBA");
        d.purchasePrice = 100000.0;
        d.salePrice = 120000.0;
        d.paymentMethod = domain::PaymentMethod::Transferencia;

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
        ConnectionPool::configure(config);

        ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
        if (!handle.database().isOpen())
            QSKIP("PostgreSQL no está disponible (docker compose up -d).");

        SqlReferenceDataReader reference(ConnectionPool::instance());
        QString error;
        m_lookups.vehicleSubtypes.clear();
        for (const auto &option : reference.vehicleCategories(&error)) {
            if (option.parentId > 0)
                m_lookups.vehicleSubtypes << option;
        }
        m_lookups.brands = reference.brands(&error);
        m_lookups.fuelTypes = reference.fuelTypes(&error);
        QVERIFY2(!m_lookups.vehicleSubtypes.isEmpty() && !m_lookups.brands.isEmpty()
                     && !m_lookups.fuelTypes.isEmpty(),
                 qPrintable(error));
    }

    void cleanupTestCase()
    {
        ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
        for (const QString &vin : std::as_const(m_createdSerials)) {
            QSqlQuery query(handle.database());
            query.prepare(QStringLiteral("DELETE FROM vehicles WHERE serial_number = :vin"));
            query.bindValue(QStringLiteral(":vin"), vin);
            query.exec();
        }
    }

    void registersAgainstTheRealDatabase()
    {
        SqlVehicleRepository vehicles(ConnectionPool::instance());
        SqlReferenceDataReader reference(ConnectionPool::instance());
        LocalFileStorage files(m_storage.path());
        PdfContractGenerator contracts;
        const application::VehicleRegistrationService service(vehicles, files, reference, contracts);

        const QString vin = QStringLiteral("TST%1").arg(QDateTime::currentMSecsSinceEpoch());
        m_createdSerials << vin;

        const auto first = service.registerVehicle(registration(vin));
        QVERIFY2(first.status == application::RegistrationResult::Status::Registered,
                 qPrintable(first.validation.joinedMessages() + first.errorMessage));
        QCOMPARE(first.status, application::RegistrationResult::Status::Registered);
        QVERIFY(first.folio > 0);
        QVERIFY(first.contract.has_value());

        // El mismo VIN otra vez: lo detecta antes de copiar.
        const auto second = service.registerVehicle(registration(vin));
        QCOMPARE(second.status, application::RegistrationResult::Status::Rejected);
        QVERIFY(second.validation.fields().contains(QStringLiteral("serialNumber")));
    }

    void lookupsComeFromTheCatalogs()
    {
        SqlVehicleRepository vehicles(ConnectionPool::instance());
        SqlReferenceDataReader reference(ConnectionPool::instance());
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
};

QTEST_GUILESS_MAIN(TstRegistrationDb)
#include "tst_registration_db.moc"
