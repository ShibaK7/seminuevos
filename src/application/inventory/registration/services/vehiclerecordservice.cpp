#include "application/inventory/registration/services/vehiclerecordservice.h"

#include "domain/inventory/model/vehicle.h"
#include "domain/inventory/model/vehiclebuilder.h"
#include "domain/inventory/rules/vehiclerecordaccess.h"
#include "registrationmapping.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <memory>

namespace application {

namespace {

// El tamaño de las miniaturas de la galería: el doble de la tarjeta de cada
// foto (360 x 280), para que se vean nítidas en pantallas con escala.
constexpr int kThumbnailWidth = 720;
constexpr int kThumbnailHeight = 560;

// Lo que ve quien no ve la operación: la tarjeta del vehículo, nada más. Se
// copia campo por campo a un DTO nuevo en vez de borrar los de la operación:
// así un dato que se agregue mañana a la operación nace oculto, en lugar de
// filtrarse hasta que alguien se acuerde de borrarlo aquí.
VehicleDetailsDto vehicleCardOnly(const VehicleDetailsDto &details)
{
    VehicleDetailsDto card;
    card.acquisitionType = details.acquisitionType;
    card.dealDate = details.dealDate;
    card.vehicleType = details.vehicleType;
    card.subtype = details.subtype;
    card.brand = details.brand;
    card.model = details.model;
    card.yearModel = details.yearModel;
    card.color = details.color;
    card.mileage = details.mileage;
    card.description = details.description;
    card.motorNumber = details.motorNumber;
    card.serialNumber = details.serialNumber;
    card.repuve = details.repuve;
    card.plates = details.plates;
    card.platesHolder = details.platesHolder;
    return card;
}

// Exactamente una portada. Si ninguna lo es, la primera, como la toma la
// tarjeta del inventario; si hay varias (nada en el esquema lo impide), la
// primera de ellas.
void keepOneCover(QList<StoredImageDto> &images)
{
    bool found = false;
    for (StoredImageDto &stored : images) {
        if (stored.image.isPrimary && !found)
            found = true;
        else
            stored.image.isPrimary = false;
    }
    if (!found && !images.isEmpty())
        images.first().image.isPrimary = true;
}

RecordUpdateResult withStatus(RecordUpdateResult::Status status)
{
    RecordUpdateResult result;
    result.status = status;
    return result;
}

RecordUpdateResult rejected(const domain::ValidationResult &validation)
{
    RecordUpdateResult result = withStatus(RecordUpdateResult::Status::Rejected);
    result.validation = validation;
    return result;
}

RecordUpdateResult rejected(const QString &field, const QString &message)
{
    domain::ValidationResult validation;
    validation.addError(field, message);
    return rejected(validation);
}

RecordUpdateResult failed(const QString &message)
{
    RecordUpdateResult result = withStatus(RecordUpdateResult::Status::Failed);
    result.errorMessage = message;
    return result;
}

RecordUpdateResult duplicateSerialNumber()
{
    return rejected(QStringLiteral("serialNumber"),
                    QStringLiteral("Ya hay otra unidad registrada con ese número de serie (VIN)."));
}

// Los archivos de una corrección. Lo que ya estaba guardado llega con la ruta
// absoluta que entregó la lectura y se conserva tal cual; lo demás es nuevo y
// se copia al almacén. Al final quedan anotados lo que se copió (por si hay
// que deshacerlo) y lo guardado que ya no aparece (para borrarlo cuando la
// base confirme).
class FileReconciler
{
public:
    FileReconciler(FileStorage &files, const VehicleRecordDto &stored)
        : m_files(files)
    {
        const auto remember = [this](const QString &relativePath) {
            if (!relativePath.isEmpty())
                m_storedByAbsolute.insert(m_files.absolutePath(relativePath), relativePath);
        };
        for (const StoredImageDto &image : stored.images)
            remember(image.image.path);
        for (const domain::VehicleDocument &document : stored.documents)
            remember(document.path);
        remember(stored.details.invoiceFilePath);
    }

    // La ruta relativa con la que `path` queda guardado: la que ya tenía, o
    // la de su copia. Vacía si la copia falló (con la causa en `error`).
    QString keepOrStore(const QString &vehicleKey, const QString &path, FileStorage::Kind kind,
                        QString &error)
    {
        const auto existing = m_storedByAbsolute.constFind(path);
        if (existing != m_storedByAbsolute.cend()) {
            m_kept.insert(existing.value());
            return existing.value();
        }
        const FileStorage::Stored copy = m_files.store(vehicleKey, path, kind);
        if (!copy.ok) {
            error = copy.errorMessage;
            return QString();
        }
        m_copied << copy.relativePath;
        return copy.relativePath;
    }

    // Deshace las copias: lo que se copió para esta corrección y no llegó a
    // la base.
    void removeCopies()
    {
        for (const QString &path : std::as_const(m_copied))
            m_files.remove(path);
    }

    // Lo guardado que ya no aparece. Solo se borra con la corrección
    // confirmada: antes, la base todavía lo nombra.
    void removeObsolete()
    {
        for (const QString &path : std::as_const(m_storedByAbsolute)) {
            if (!m_kept.contains(path) && !m_files.remove(path))
                qWarning("No se pudo borrar del almacén %s: queda huérfano.", qUtf8Printable(path));
        }
    }

private:
    FileStorage &m_files;
    QHash<QString, QString> m_storedByAbsolute; // absoluta -> relativa
    QSet<QString> m_kept;
    QStringList m_copied;
};

} // namespace

VehicleRecordService::VehicleRecordService(VehicleRecordReader &records,
                                           VehicleRepository &vehicles, FileStorage &files,
                                           ReferenceDataReader &referenceData)
    : m_records(records)
    , m_vehicles(vehicles)
    , m_files(files)
    , m_referenceData(referenceData)
{
}

RecordAccessDto VehicleRecordService::accessFor(domain::UserRole role) const
{
    const domain::VehicleRecordAccess access(role);
    RecordAccessDto dto;
    dto.canEdit = access.canEdit();
    dto.showsDealTerms = access.canSeeDealTerms();
    dto.showsMaintenanceHistory = access.canSeeMaintenanceHistory();
    return dto;
}

RecordLoadResult VehicleRecordService::load(int folio, domain::UserRole role) const
{
    RecordLoadResult result;
    QString error;
    std::optional<VehicleRecordDto> record = m_records.findByFolio(folio, &error);
    if (!record) {
        if (error.isEmpty()) {
            result.status = RecordLoadResult::Status::NotFound;
        } else {
            result.status = RecordLoadResult::Status::Failed;
            result.errorMessage = error;
        }
        return result;
    }

    // La base guarda rutas relativas al almacén; la pantalla las necesita
    // absolutas para abrirlas ("Ver") y para volver a guardarlas tal cual.
    keepOneCover(record->images);
    for (StoredImageDto &stored : record->images) {
        stored.image.path = m_files.absolutePath(stored.image.path);
        stored.thumbnail = m_files.readThumbnail(stored.image.path, kThumbnailWidth,
                                                 kThumbnailHeight);
    }
    for (domain::VehicleDocument &document : record->documents) {
        if (!document.path.isEmpty())
            document.path = m_files.absolutePath(document.path);
    }

    if (!accessFor(role).showsDealTerms) {
        record->details = vehicleCardOnly(record->details);
    } else if (!record->details.invoiceFilePath.isEmpty()) {
        record->details.invoiceFilePath = m_files.absolutePath(record->details.invoiceFilePath);
    }

    result.status = RecordLoadResult::Status::Loaded;
    result.record = *record;
    return result;
}

RecordUpdateResult VehicleRecordService::update(const VehicleUpdateDto &update,
                                                domain::UserRole role) const
{
    if (!accessFor(role).canEdit)
        return failed(QStringLiteral("Tu rol no permite corregir el expediente."));

    // La UMA se lee al guardar, como en el alta: no se confía en la que tenía
    // la pantalla.
    QString umaError;
    const std::optional<double> uma = m_referenceData.umaDailyValue(&umaError);
    if (!umaError.isEmpty())
        return failed(QStringLiteral("No se pudo leer la UMA: %1").arg(umaError));

    // 1. Las reglas del dominio, sin tocar disco ni base.
    domain::VehicleBuilder builder;
    domain::ValidationResult mappingErrors = mapping::applyDetails(builder, update.details, uma);
    mappingErrors.merge(mapping::applyConditions(builder, update.conditions));
    mapping::applyFiles(builder, update.files);

    domain::ValidationResult validation;
    std::unique_ptr<domain::Vehicle> vehicle = builder.build(validation);
    validation.merge(mappingErrors);
    if (!vehicle || !validation.isValid())
        return rejected(validation);
    vehicle->assignFolio(update.folio);

    // 2. Lo guardado: que siga ahí, como se leyó, y qué archivos tiene.
    QString readError;
    const std::optional<VehicleRecordDto> stored = m_records.findByFolio(update.folio, &readError);
    if (!stored) {
        if (!readError.isEmpty())
            return failed(QStringLiteral("No se pudo leer la unidad: %1").arg(readError));
        return withStatus(RecordUpdateResult::Status::NotFound);
    }
    if (stored->version != update.version)
        return withStatus(RecordUpdateResult::Status::Conflict);
    if (!stored->hasDealTerms) {
        return failed(QStringLiteral("La unidad no tiene registrados los datos de su compra o "
                                     "consignación: no se puede corregir."));
    }
    if (stored->details.acquisitionType != vehicle->acquisitionType()) {
        return rejected(QStringLiteral("acquisitionType"),
                        QStringLiteral("El tipo de operación no se cambia al editar el expediente."));
    }

    // 3. Un VIN de otra unidad lo rechazaría la base, pero ya con los
    // archivos copiados. Se pregunta antes.
    QString lookupError;
    if (m_vehicles.serialNumberExistsElsewhere(vehicle->serialNumber(), update.folio, &lookupError))
        return duplicateSerialNumber();
    if (!lookupError.isEmpty())
        return failed(QStringLiteral("No se pudo revisar el número de serie: %1").arg(lookupError));

    // 4. Los archivos: lo que ya estaba se conserva, lo nuevo se copia.
    FileReconciler files(m_files, *stored);
    const QString key = vehicle->serialNumber();
    QString storeError;
    if (!vehicle->invoiceFilePath().isEmpty()) {
        const QString path = files.keepOrStore(key, vehicle->invoiceFilePath(),
                                               FileStorage::Kind::Document, storeError);
        if (path.isEmpty()) {
            files.removeCopies();
            return failed(QStringLiteral("Error guardando la factura: %1").arg(storeError));
        }
        vehicle->setInvoiceFilePath(path);
    }
    for (int i = 0; i < vehicle->images().size(); ++i) {
        const QString path = files.keepOrStore(key, vehicle->images().at(i).path,
                                               FileStorage::Kind::Image, storeError);
        if (path.isEmpty()) {
            files.removeCopies();
            return failed(QStringLiteral("Error guardando una fotografía: %1").arg(storeError));
        }
        (void)vehicle->setImageStoredPath(i, path);
    }
    for (int i = 0; i < vehicle->documents().size(); ++i) {
        const QString path = files.keepOrStore(key, vehicle->documents().at(i).path,
                                               FileStorage::Kind::Document, storeError);
        if (path.isEmpty()) {
            files.removeCopies();
            return failed(QStringLiteral("Error guardando un documento: %1").arg(storeError));
        }
        (void)vehicle->setDocumentStoredPath(i, path);
    }

    // 5. Todo o nada.
    const VehicleRepository::UpdateOutcome outcome = m_vehicles.update(*vehicle, update.version);
    if (!outcome.ok) {
        // Si falló el COMMIT no se sabe si quedó guardado: se deja todo.
        if (!outcome.commitFailed)
            files.removeCopies();
        if (outcome.notFound)
            return withStatus(RecordUpdateResult::Status::NotFound);
        if (outcome.conflict)
            return withStatus(RecordUpdateResult::Status::Conflict);
        if (outcome.duplicateSerialNumber)
            return duplicateSerialNumber();
        if (!outcome.takenNationalIdOwner.isEmpty()) {
            return rejected(QStringLiteral("counterparty.nationalId"),
                            QStringLiteral("Esa identificación ya es de «%1». Revisa el nombre o "
                                           "la identificación.")
                                .arg(outcome.takenNationalIdOwner));
        }
        return failed(outcome.errorMessage);
    }

    // 6. Ya confirmado: lo que dejó de usarse sale del almacén.
    files.removeObsolete();
    return withStatus(RecordUpdateResult::Status::Saved);
}

} // namespace application
