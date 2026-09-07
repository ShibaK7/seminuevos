#include "../../include/db/vehicleregistrationworker.h"
#include "../../include/db/connectionpool.h"
#include "../../include/db/vehicleregistrationservice.h"
#include "../../include/storage/localfilestoragemanager.h"

#include <QSqlDatabase>
#include <QSqlError>

VehicleRegistrationWorker::VehicleRegistrationWorker(VehicleDraft draft, QString storageRoot, QObject *parent)
    : QThread(parent)
    , m_draft(std::move(draft))
    , m_storageRoot(std::move(storageRoot))
{
}

void VehicleRegistrationWorker::run()
{
    const LocalFileStorageManager storage(m_storageRoot);
    QString errorMessage;

    if (!m_draft.invoiceFilePath.isEmpty()) {
        const QString relativePath = storage.saveVehicleFile(
            m_draft.serialNumber, m_draft.invoiceFilePath, QStringLiteral("documents"), errorMessage);
        if (relativePath.isEmpty()) {
            emit registrationFailed(QStringLiteral("Error guardando la factura: %1").arg(errorMessage));
            return;
        }
        m_draft.invoiceFilePath = relativePath;
    }

    for (PendingImage &image : m_draft.images) {
        const QString relativePath = storage.saveVehicleImageCompressed(
            m_draft.serialNumber, image.sourcePath, QStringLiteral("images"), errorMessage);
        if (relativePath.isEmpty()) {
            emit registrationFailed(QStringLiteral("Error guardando una fotografía: %1").arg(errorMessage));
            return;
        }
        image.sourcePath = relativePath;
    }

    for (PendingDocument &document : m_draft.documents) {
        const QString relativePath = storage.saveVehicleFile(
            m_draft.serialNumber, document.sourcePath, QStringLiteral("documents"), errorMessage);
        if (relativePath.isEmpty()) {
            emit registrationFailed(QStringLiteral("Error guardando un documento: %1").arg(errorMessage));
            return;
        }
        document.sourcePath = relativePath;
    }

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();
    if (!db.isOpen()) {
        emit registrationFailed(QStringLiteral("No se pudo conectar con la base de datos: %1").arg(db.lastError().text()));
        return;
    }

    const VehicleRegistrationService::Result result = VehicleRegistrationService::insertVehicle(db, m_draft);
    if (!result.ok) {
        emit registrationFailed(result.errorMessage);
        return;
    }

    emit registrationSucceeded(result.folio);
}
