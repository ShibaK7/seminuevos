#include "application/inventory/registration/services/vehiclerecordservice.h"

#include "domain/inventory/rules/vehiclerecordaccess.h"

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

} // namespace

VehicleRecordService::VehicleRecordService(VehicleRecordReader &records, FileStorage &files)
    : m_records(records)
    , m_files(files)
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

} // namespace application
