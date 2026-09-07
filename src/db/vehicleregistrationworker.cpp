#include "../../include/db/vehicleregistrationworker.h"

#include "../../include/db/connectionpool.h"
#include "../../include/db/vehiclerepository.h"
#include "../../include/storage/localfilestoragemanager.h"
#include "domain/vehicle.h"

#include <QSqlDatabase>
#include <QSqlError>

#include <utility>

VehicleRegistrationWorker::VehicleRegistrationWorker(std::unique_ptr<domain::Vehicle> vehicle,
                                                     QString storageRoot, QObject *parent)
    : QThread(parent)
    , m_vehicle(std::move(vehicle))
    , m_storageRoot(std::move(storageRoot))
{
}

// Fuera de línea: el destructor de unique_ptr<domain::Vehicle> necesita ver
// la definición completa de Vehicle, y el header solo la declara.
VehicleRegistrationWorker::~VehicleRegistrationWorker() = default;

void VehicleRegistrationWorker::run()
{
    if (!m_vehicle) {
        emit registrationFailed(QStringLiteral("No se recibió ningún vehículo que registrar."));
        return;
    }

    const LocalFileStorageManager storage(m_storageRoot);
    const QString vin = m_vehicle->serialNumber();
    QString errorMessage;

    // Los archivos se copian ANTES de abrir la transacción, para no sostenerla
    // durante operaciones de disco.
    if (!m_vehicle->invoiceFilePath().isEmpty()) {
        const QString relativePath = storage.saveVehicleFile(
            vin, m_vehicle->invoiceFilePath(), QStringLiteral("documents"), errorMessage);
        if (relativePath.isEmpty()) {
            emit registrationFailed(QStringLiteral("Error guardando la factura: %1").arg(errorMessage));
            return;
        }
        m_vehicle->setInvoiceFilePath(relativePath);
    }

    for (int i = 0; i < m_vehicle->images().size(); ++i) {
        const QString relativePath = storage.saveVehicleImageCompressed(
            vin, m_vehicle->images().at(i).path, QStringLiteral("images"), errorMessage);
        if (relativePath.isEmpty()) {
            emit registrationFailed(QStringLiteral("Error guardando una fotografía: %1").arg(errorMessage));
            return;
        }
        if (!m_vehicle->setImageStoredPath(i, relativePath)) {
            emit registrationFailed(QStringLiteral("No se pudo registrar la ruta de una fotografía."));
            return;
        }
    }

    for (int i = 0; i < m_vehicle->documents().size(); ++i) {
        const QString relativePath = storage.saveVehicleFile(
            vin, m_vehicle->documents().at(i).path, QStringLiteral("documents"), errorMessage);
        if (relativePath.isEmpty()) {
            emit registrationFailed(QStringLiteral("Error guardando un documento: %1").arg(errorMessage));
            return;
        }
        if (!m_vehicle->setDocumentStoredPath(i, relativePath)) {
            emit registrationFailed(QStringLiteral("No se pudo registrar la ruta de un documento."));
            return;
        }
    }

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();
    if (!db.isOpen()) {
        emit registrationFailed(
            QStringLiteral("No se pudo conectar con la base de datos: %1").arg(db.lastError().text()));
        return;
    }

    VehicleRepository repository(db);
    const VehicleRepository::Result result = repository.save(*m_vehicle);
    if (!result.ok) {
        emit registrationFailed(result.errorMessage);
        return;
    }

    emit registrationSucceeded(result.folio);
}
