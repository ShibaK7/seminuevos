// Pruebas del caso de uso "registrar vehículo" con puertos falsos: sin base de
// datos ni disco. Fijan el orden que evita archivos huérfanos (validar, revisar
// el VIN, copiar, guardar) y la compensación cuando algo falla después de
// copiar.

#include "application/inventory/registration/services/vehicleregistrationservice.h"
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

    // La cuenta que pagaría la compra no alcanza al guardar (otra sesión gastó
    // el dinero): rechazo en el precio, y lo copiado se borra.
    void insufficientFundsOnSaveAreReportedOnThePrice()
    {
        m_vehicles.insufficientOnAdd = 5000000;
        const auto result = service().registerVehicle(validRegistration(domain::AcquisitionType::Adquisicion));
        QCOMPARE(result.status, application::RegistrationResult::Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("purchasePrice")});
        QCOMPARE(result.validation.firstMessage(),
                 QStringLiteral("Cantidad Banco solo tiene $50,000.00 disponibles."));
        QCOMPARE(m_files.removed, m_files.stored);
    }

    // El paso Detalles no deja pasar una compra de contado mayor a lo que hay
    // en la cuenta de su método; a crédito, en consignación o sin saldos
    // leídos, no se revisa.
    void detailsStepIsLimitedByTheAvailableCash()
    {
        auto details = validRegistration(domain::AcquisitionType::Adquisicion).details;
        details.paymentType = domain::PaymentType::Contado;
        details.paymentMethod = domain::PaymentMethod::Transferencia;
        details.purchasePrice = 100000.0;
        const domain::CashLimitTerms terms{117.31, 3210};
        const QMap<domain::CashAccount, qint64> available{{domain::CashAccount::PettyCash, 99999999},
                                                          {domain::CashAccount::Bank, 9999999}};

        const auto tooMuch = service().validateDetails(details, terms, available);
        QCOMPARE(tooMuch.fields(), QStringList{QStringLiteral("purchasePrice")});
        QCOMPARE(tooMuch.firstMessage(), QStringLiteral("Cantidad Banco solo tiene $99,999.99 disponibles."));

        details.purchasePrice = 99999.99;
        QVERIFY(service().validateDetails(details, terms, available).isValid());

        details.purchasePrice = 100000.0;
        details.paymentType = domain::PaymentType::Credito;
        QVERIFY(service().validateDetails(details, terms, available).isValid());
        details.paymentType = domain::PaymentType::Contado;
        QVERIFY(service().validateDetails(details, terms).isValid());
    }

    void lookupsCarryTheAvailableCash()
    {
        m_reference.cash = {{domain::CashAccount::PettyCash, 100}, {domain::CashAccount::Bank, 200}};
        const auto lookups = service().loadLookups();
        QCOMPARE(lookups.cashAvailable.value(domain::CashAccount::Bank), qint64(200));
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

    // El tope que fijó el Administrador manda al guardar: con 1,000 UMA, una
    // compra de 150,000 en efectivo ya no pasa (el tope es 117,310).
    void configuredCashLimitIsUsedOnSave()
    {
        m_reference.umaMultiple = 1000;
        auto dto = validRegistration(domain::AcquisitionType::Adquisicion);
        dto.details.paymentMethod = domain::PaymentMethod::Efectivo;
        dto.details.purchasePrice = 150000.0;
        const auto result = service().registerVehicle(dto);
        QCOMPARE(result.status, application::RegistrationResult::Status::Rejected);
        QVERIFY2(result.validation.joinedMessages().contains(QStringLiteral("1,000 UMA")),
                 qPrintable(result.validation.joinedMessages()));
    }

    // El paso del asistente valida con el tope que le dieron (el de los
    // catálogos), no con el legal.
    void detailsStepUsesTheGivenCashLimit()
    {
        auto details = validRegistration(domain::AcquisitionType::Adquisicion).details;
        details.paymentMethod = domain::PaymentMethod::Efectivo;
        details.purchasePrice = 150000.0;
        QVERIFY(!service()
                     .validateDetails(details, domain::CashLimitTerms{117.31, 3210})
                     .fields()
                     .contains(QStringLiteral("paymentMethod")));
        const domain::ValidationResult strict =
            service().validateDetails(details, domain::CashLimitTerms{117.31, 1000});
        QVERIFY2(strict.fields().contains(QStringLiteral("paymentMethod")),
                 qPrintable(strict.joinedMessages()));
    }

    void lookupsSplitTypesAndSubtypes()
    {
        const auto lookups = service().loadLookups();
        QCOMPARE(lookups.vehicleTypes.size(), 1);
        QCOMPARE(lookups.vehicleSubtypes.size(), 1);
        QCOMPARE(lookups.vehicleSubtypes.first().parentId, 1);
        QVERIFY(lookups.cashLimit.hasUma());
        QCOMPARE(lookups.cashLimit.umaMultiple, 3210);
        QVERIFY(lookups.errorMessage.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstRegistrationService)
#include "tst_registrationservice.moc"
