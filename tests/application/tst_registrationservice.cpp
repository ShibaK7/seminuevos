// Pruebas del caso de uso "registrar vehículo" con puertos falsos: sin base de
// datos ni disco. Fijan el orden que evita archivos huérfanos (validar, revisar
// el VIN, copiar, guardar) y la compensación cuando algo falla después de
// copiar.

#include "application/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "registrationfixtures.h"

#include <QtTest>

using fixtures::validRegistration;

class TstRegistrationService : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryVehicleRepository m_vehicles;
    fakes::FakeFileStorage m_files;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeContractGenerator m_contracts;

    application::VehicleRegistrationService service()
    {
        return application::VehicleRegistrationService(m_vehicles, m_files, m_reference, m_contracts);
    }

private slots:
    void init()
    {
        m_vehicles = fakes::InMemoryVehicleRepository();
        m_files = fakes::FakeFileStorage();
        m_reference = fakes::FakeReferenceDataReader();
        m_contracts = fakes::FakeContractGenerator();
    }

    void registersAndReturnsFolioAndContract()
    {
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Adquisicion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Registered);
        QCOMPARE(result.folio, 100);
        QVERIFY(result.contract.has_value());
        QCOMPARE(result.contract->amount, 150000.0);
        QCOMPARE(m_files.stored.size(), 2); // foto + documento
        QVERIFY(m_files.removed.isEmpty());
    }

    // Mismo contrato para las dos ramas: en consignación declara el precio base.
    void consignmentContractDeclaresTheBasePrice()
    {
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Consignacion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Registered);
        QVERIFY(result.contract.has_value());
        QCOMPARE(result.contract->amount, 200000.0);
    }

    void invalidDataIsRejectedWithoutAnyIO()
    {
        auto dto = validRegistration(domain::AcquisitionType::Adquisicion);
        dto.details.serialNumber.clear();
        const auto result = service().registerVehicle(dto);
        QCOMPARE(result.status, application::RegistrationResult::Status::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("serialNumber")));
        QVERIFY(m_files.stored.isEmpty());
        QCOMPARE(m_vehicles.adds, 0);
    }

    // Un VIN repetido se detecta ANTES de copiar: no quedan archivos huérfanos.
    void duplicateSerialNumberCopiesNothing()
    {
        m_vehicles.existingSerialNumbers << QStringLiteral("3N1CN7AD0LL000001");
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Adquisicion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("serialNumber")));
        QVERIFY(m_files.stored.isEmpty());
    }

    // Si falla la segunda copia, se borra la primera y no se guarda nada.
    void failedCopyRemovesWhatWasCopied()
    {
        m_files.failAtStore = 2;
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Adquisicion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Failed);
        QCOMPARE(m_files.removed, m_files.stored);
        QCOMPARE(m_files.removed.size(), 1);
        QCOMPARE(m_vehicles.adds, 0);
    }

    // Si la base falla después de copiar, se borra exactamente lo copiado.
    void failedSaveRemovesExactlyWhatWasCopied()
    {
        m_vehicles.failNextAdd = true;
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Adquisicion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Failed);
        QCOMPARE(m_files.removed, m_files.stored);
        QCOMPARE(m_files.removed.size(), 2);
    }

    // Si la base lo rechaza por VIN duplicado (otra estación lo guardó entre la
    // consulta y el guardado), se reporta en el campo y se limpian los archivos.
    void duplicateOnSaveIsReportedOnTheField()
    {
        m_vehicles.duplicateOnAdd = true;
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Adquisicion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("serialNumber")));
        QCOMPARE(m_files.removed, m_files.stored);
    }

    void cashWithoutUmaIsRejected()
    {
        m_reference.uma.reset();
        auto dto = validRegistration(domain::AcquisitionType::Adquisicion);
        dto.details.paymentMethod = domain::PaymentMethod::Efectivo;
        const auto result = service().registerVehicle(dto);
        QCOMPARE(result.status, application::RegistrationResult::Status::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("paymentMethod")));
    }

    void lookupsSplitTypesAndSubtypes()
    {
        const auto lookups = service().loadLookups();
        QCOMPARE(lookups.vehicleTypes.size(), 1);
        QCOMPARE(lookups.vehicleSubtypes.size(), 1);
        QCOMPARE(lookups.vehicleSubtypes.first().parentId, 1);
        QVERIFY(lookups.umaDailyValue.has_value());
        QVERIFY(lookups.errorMessage.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstRegistrationService)
#include "tst_registrationservice.moc"
