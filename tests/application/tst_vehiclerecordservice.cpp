// Caso de uso del expediente de un vehículo (US-03.3 y US-03.4) con puertos
// falsos: qué deja ver cada rol y cómo llega lo guardado a la pantalla.

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "fakes.h"
#include "registrationfixtures.h"

#include <QtTest>

using Status = application::RecordLoadResult::Status;
using UpdateStatus = application::RecordUpdateResult::Status;
using domain::UserRole;

class TstVehicleRecordService : public QObject
{
    Q_OBJECT

private:
    fakes::FakeVehicleRecordReader m_reader;
    fakes::InMemoryVehicleRepository m_vehicles;
    fakes::FakeFileStorage m_files;
    fakes::FakeReferenceDataReader m_reference;

    application::VehicleRecordService service()
    {
        return {m_reader, m_vehicles, m_files, m_reference};
    }

    // Lo que la pantalla manda a guardar si nadie cambia nada: lo leído, con
    // sus rutas absolutas.
    application::VehicleUpdateDto editedRecord()
    {
        const application::RecordLoadResult loaded = service().load(7, UserRole::Admin);
        application::VehicleUpdateDto update;
        update.folio = 7;
        update.version = loaded.record.version;
        update.loadedCounterparty = loaded.record.details.counterparty;
        update.counterpartyVersion = loaded.record.counterpartyVersion;
        update.details = loaded.record.details;
        update.conditions = loaded.record.conditions;
        for (const application::StoredImageDto &stored : loaded.record.images)
            update.files.images << stored.image;
        update.files.documents = loaded.record.documents;
        return update;
    }

    static QString stored(const QString &relative)
    {
        return QStringLiteral("vehicles/VIN1/") + relative;
    }

private slots:
    void init()
    {
        m_reader = fakes::FakeVehicleRecordReader();
        m_vehicles = fakes::InMemoryVehicleRepository();
        m_files = fakes::FakeFileStorage();
        m_reference = fakes::FakeReferenceDataReader();
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

    // --- Guardar lo corregido (#34) ---

    void sellerCannotSave()
    {
        const auto result = service().update(editedRecord(), UserRole::SalesAgent);
        QCOMPARE(result.status, UpdateStatus::Failed);
        QCOMPARE(m_vehicles.updates, 0);
    }

    // Lo que no pasa las reglas se rechaza sin tocar disco ni base.
    void invalidDataIsRejectedWithoutIO()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.details.model.clear();
        const qsizetype reads = m_reader.reads.size();

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(result.status, UpdateStatus::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("model")));
        QCOMPARE(m_reader.reads.size(), reads);
        QVERIFY(m_files.stored.isEmpty());
        QCOMPARE(m_vehicles.updates, 0);
    }

    // Si otro usuario la cambió desde que se leyó, no se copia nada.
    void staleVersionConflictsBeforeCopying()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/3.jpg"), false};
        m_reader.records[7].version = QStringLiteral("1002");

        QCOMPARE(service().update(update, UserRole::Admin).status, UpdateStatus::Conflict);
        QVERIFY(m_files.stored.isEmpty());
        QCOMPARE(m_vehicles.updates, 0);
    }

    void missingFolioIsNotFoundOnSave()
    {
        const application::VehicleUpdateDto update = editedRecord();
        m_reader.records.remove(7);
        QCOMPARE(service().update(update, UserRole::Admin).status, UpdateStatus::NotFound);
        QCOMPARE(m_vehicles.updates, 0);
    }

    // La rama no se cambia al editar: la pantalla la bloquea, y el servicio
    // tampoco la acepta.
    void branchCannotChange()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.details.acquisitionType = domain::AcquisitionType::Consignacion;
        update.details.invoiceType = domain::InvoiceType::FacturadoReal;
        update.details.basePrice = 200000.0;

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(result.status, UpdateStatus::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("acquisitionType")));
        QCOMPARE(m_vehicles.updates, 0);
    }

    // El VIN de otra unidad se rechaza antes de copiar; el propio no cuenta
    // (lo resuelve el repositorio, que excluye a la unidad).
    void foreignVinIsRejectedBeforeCopying()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/3.jpg"), false};
        m_vehicles.existingSerialNumbers << update.details.serialNumber;

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(result.status, UpdateStatus::Rejected);
        QVERIFY(result.validation.fields().contains(QStringLiteral("serialNumber")));
        QVERIFY(m_files.stored.isEmpty());
    }

    // Lo que ya estaba se conserva con su ruta, lo nuevo se copia y lo que ya
    // no aparece se borra del almacén, solo después de guardar.
    void keepsStoresAndRemovesFiles()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.files.images.removeFirst();
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/3.jpg"), false};
        update.details.invoiceFilePath = QStringLiteral("C:/nuevas/factura2.pdf");

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(result.status, UpdateStatus::Saved);
        QCOMPARE(m_vehicles.updatedFolio, 7);
        QCOMPARE(m_vehicles.updatedVersion, QStringLiteral("1001"));

        QCOMPARE(m_files.stored.size(), 2);
        QCOMPARE(m_vehicles.updatedInvoicePath, m_files.stored.at(0));
        QCOMPARE(m_vehicles.updatedImages.size(), 2);
        QCOMPARE(m_vehicles.updatedImages.at(0).path, stored(QStringLiteral("images/2.jpg")));
        QVERIFY(m_vehicles.updatedImages.at(0).isPrimary);
        QCOMPARE(m_vehicles.updatedImages.at(1).path, m_files.stored.at(1));
        QCOMPARE(m_vehicles.updatedDocuments.first().path, stored(QStringLiteral("documents/tarjeta.pdf")));

        QStringList removed = m_files.removed;
        removed.sort();
        QCOMPARE(removed, (QStringList{stored(QStringLiteral("documents/factura.pdf")),
                                       stored(QStringLiteral("images/1.jpg"))}));
    }

    // Quitar la factura la borra del almacén al guardar.
    void removedInvoiceLeavesTheStore()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.details.invoiceFilePath.clear();

        QCOMPARE(service().update(update, UserRole::Admin).status, UpdateStatus::Saved);
        QVERIFY(m_vehicles.updatedInvoicePath.isEmpty());
        QCOMPARE(m_files.removed, QStringList{stored(QStringLiteral("documents/factura.pdf"))});
    }

    // La contraparte solo se escribe si el usuario la cambió: otro usuario
    // pudo corregirla mientras tanto desde otra unidad de la misma persona.
    void untouchedCounterpartyIsNotWritten()
    {
        QCOMPARE(service().update(editedRecord(), UserRole::Admin).status, UpdateStatus::Saved);
        QVERIFY(!m_vehicles.updatedExpected.counterpartyEdited);
        QCOMPARE(m_vehicles.updatedExpected.vehicleVersion, QStringLiteral("1001"));
    }

    void editedCounterpartyCarriesItsVersion()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.details.counterparty.locality = QStringLiteral("Pátzcuaro");
        QCOMPARE(service().update(update, UserRole::Admin).status, UpdateStatus::Saved);
        QVERIFY(m_vehicles.updatedExpected.counterpartyEdited);
        QCOMPARE(m_vehicles.updatedExpected.counterpartyVersion, QStringLiteral("2001"));
    }

    // Sin cambios en los archivos, no se copia ni se borra nada.
    void unchangedFilesAreLeftAlone()
    {
        QCOMPARE(service().update(editedRecord(), UserRole::Admin).status, UpdateStatus::Saved);
        QVERIFY(m_files.stored.isEmpty());
        QVERIFY(m_files.removed.isEmpty());
        QCOMPARE(m_vehicles.updatedInvoicePath, stored(QStringLiteral("documents/factura.pdf")));
    }

    // Si la base no lo guarda, se borra solo lo que se copió; lo guardado se
    // queda donde estaba.
    void failedSaveRemovesOnlyTheCopies_data()
    {
        QTest::addColumn<int>("outcome");
        QTest::addColumn<int>("status");
        QTest::addColumn<QString>("field");
        QTest::newRow("falla") << 0 << static_cast<int>(UpdateStatus::Failed) << QString();
        QTest::newRow("conflicto") << 1 << static_cast<int>(UpdateStatus::Conflict) << QString();
        QTest::newRow("no existe") << 2 << static_cast<int>(UpdateStatus::NotFound) << QString();
        QTest::newRow("VIN") << 3 << static_cast<int>(UpdateStatus::Rejected)
                             << QStringLiteral("serialNumber");
        QTest::newRow("identificación") << 4 << static_cast<int>(UpdateStatus::Rejected)
                                        << QStringLiteral("counterparty.nationalId");
    }

    void failedSaveRemovesOnlyTheCopies()
    {
        QFETCH(int, outcome);
        QFETCH(int, status);
        QFETCH(QString, field);
        application::VehicleRepository::UpdateOutcome failure;
        failure.errorMessage = QStringLiteral("server closed the connection");
        failure.conflict = outcome == 1;
        failure.notFound = outcome == 2;
        failure.duplicateSerialNumber = outcome == 3;
        if (outcome == 4)
            failure.takenNationalIdOwner = QStringLiteral("Ana Prueba");
        m_vehicles.updateOutcome = failure;

        application::VehicleUpdateDto update = editedRecord();
        update.files.images.removeFirst();
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/3.jpg"), false};

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(static_cast<int>(result.status), status);
        if (!field.isEmpty())
            QVERIFY(result.validation.fields().contains(field));
        if (outcome == 4)
            QVERIFY(result.validation.joinedMessages().contains(QStringLiteral("Ana Prueba")));
        QCOMPARE(m_files.stored.size(), 1);
        QCOMPARE(m_files.removed, m_files.stored);
    }

    // Si falló el COMMIT no se sabe si quedó guardado: no se borra nada, ni
    // lo copiado ni lo que dejó de usarse, y se dice que no está confirmado
    // (no que falló): reintentar sobre la versión vieja no sirve.
    void commitFailureRemovesNothing()
    {
        application::VehicleRepository::UpdateOutcome failure;
        failure.commitFailed = true;
        failure.errorMessage = QStringLiteral("could not commit");
        m_vehicles.updateOutcome = failure;

        application::VehicleUpdateDto update = editedRecord();
        update.files.images.removeFirst();
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/3.jpg"), false};

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(result.status, UpdateStatus::Unconfirmed);
        QCOMPARE(result.errorMessage, QStringLiteral("could not commit"));
        QVERIFY(m_files.removed.isEmpty());
    }

    // Si una copia falla, se deshacen las que ya se hicieron.
    void failedCopyUndoesTheOthers()
    {
        application::VehicleUpdateDto update = editedRecord();
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/3.jpg"), false};
        update.files.images << domain::VehicleImage{QStringLiteral("C:/nuevas/4.jpg"), false};
        m_files.failAtStore = 2;

        const auto result = service().update(update, UserRole::Admin);
        QCOMPARE(result.status, UpdateStatus::Failed);
        QCOMPARE(m_files.removed, m_files.stored);
        QCOMPARE(m_vehicles.updates, 0);
    }
};

QTEST_APPLESS_MAIN(TstVehicleRecordService)
#include "tst_vehiclerecordservice.moc"
