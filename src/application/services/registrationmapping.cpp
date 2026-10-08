#include "registrationmapping.h"

#include "domain/model/counterparty.h"
#include "domain/model/inspection.h"
#include "domain/model/inspectionitem.h"
#include "domain/model/vehiclebuilder.h"
#include "domain/model/vehicleconditions.h"

namespace application::mapping {

namespace {
void reject(domain::ValidationResult &result, bool accepted, const QString &field,
            const QString &message)
{
    if (!accepted)
        result.addError(field, message);
}
} // namespace

domain::ValidationResult applyDetails(domain::VehicleBuilder &builder,
                                      const VehicleDetailsDto &details,
                                      std::optional<double> umaDailyValue)
{
    domain::ValidationResult rejected;

    // El tipo de operación va primero: decide qué subclase construye el
    // builder, y todo lo demás se aplica sobre ella.
    builder.setAcquisitionType(details.acquisitionType);

    builder.setDealDate(details.dealDate)
        .setVehicleType(details.vehicleType)
        .setSubtype(details.subtype)
        .setBrand(details.brand)
        .setModel(details.model)
        .setYearModel(details.yearModel)
        .setColor(details.color)
        .setMileage(details.mileage)
        .setDescription(details.description)
        .setMotorNumber(details.motorNumber)
        .setSerialNumber(details.serialNumber)
        .setRepuve(details.repuve)
        .setPlates(details.plates)
        .setPlatesHolder(details.platesHolder);

    const CounterpartyDto &c = details.counterparty;
    domain::Counterparty owner;
    reject(rejected, owner.setFullName(c.fullName), QStringLiteral("counterparty.fullName"),
           QStringLiteral("El nombre es demasiado largo."));
    reject(rejected, owner.setNationalId(c.nationalId), QStringLiteral("counterparty.nationalId"),
           QStringLiteral("La identificación es demasiado larga."));
    owner.setStreetAddress(c.streetAddress);
    reject(rejected, owner.setSuburb(c.suburb), QStringLiteral("counterparty.suburb"),
           QStringLiteral("La colonia es demasiado larga."));
    reject(rejected, owner.setLocality(c.locality), QStringLiteral("counterparty.locality"),
           QStringLiteral("La localidad es demasiado larga."));
    reject(rejected, owner.setState(c.state), QStringLiteral("counterparty.state"),
           QStringLiteral("El estado es demasiado largo."));
    reject(rejected, owner.setPostalCode(c.postalCode), QStringLiteral("counterparty.postalCode"),
           QStringLiteral("El código postal debe tener 5 dígitos."));
    builder.setCounterparty(owner);

    if (details.invoiceType)
        builder.setInvoiceType(*details.invoiceType);
    builder.setInvoiceNumber(details.invoiceNumber)
        .setInvoiceIssuer(details.invoiceIssuer)
        .setMaintenanceCost(details.maintenanceCost)
        .setObservations(details.observations);

    // Los datos de cada rama se mandan siempre: el builder ignora los que no
    // corresponden a la subclase que construyó.
    builder.setInvoiceFilePath(details.invoiceFilePath)
        .setSalePrice(details.salePrice)
        .setPaymentType(details.paymentType)
        .setPaymentMethod(details.paymentMethod)
        .setUmaDailyValue(umaDailyValue.value_or(0.0))
        .setCommissionRate(details.commissionRate);

    // Los precios de compra y base en cero no se mandan: cero solo significa
    // "sin capturar", y mandarlo haría que el setter lo rechazara y el mismo
    // problema se reportara dos veces.
    if (details.purchasePrice > 0.0)
        builder.setPurchasePrice(details.purchasePrice);
    if (details.basePrice > 0.0)
        builder.setBasePrice(details.basePrice);

    return rejected;
}

domain::ValidationResult applyConditions(domain::VehicleBuilder &builder,
                                         const VehicleConditionsDto &dto)
{
    domain::ValidationResult rejected;

    domain::VehicleConditions conditions;
    conditions.setFuelType(dto.fuelType);
    // Lo que no se eligió no se vuelca: así el dominio lo reporta como
    // faltante en vez de recibir un valor inventado.
    if (dto.cylinders) {
        reject(rejected, conditions.setCylinders(*dto.cylinders),
               QStringLiteral("conditions.cylinders"), QStringLiteral("Número de cilindros inválido."));
    }
    if (dto.transmission)
        conditions.setTransmission(*dto.transmission);
    reject(rejected, conditions.setInteriorMaterial(dto.interiorMaterial),
           QStringLiteral("conditions.interiorMaterial"),
           QStringLiteral("El material de interiores es demasiado largo."));
    if (dto.windowRegulators)
        conditions.setWindowRegulators(*dto.windowRegulators);
    if (dto.airConditioning)
        conditions.setAirConditioning(*dto.airConditioning);
    builder.setConditions(conditions);

    domain::Inspection inspection;
    for (const InspectionEntryDto &entry : dto.inspection) {
        domain::InspectionItem item;
        reject(rejected, item.setElementId(entry.elementId), QStringLiteral("inspection"),
               QStringLiteral("Hay un renglón del checklist sin elemento."));
        item.setOptimal(entry.isOptimal);
        (void)item.setObservations(entry.observations);
        inspection.setItem(item);
    }
    builder.setInspection(inspection);

    return rejected;
}

void applyFiles(domain::VehicleBuilder &builder, const VehicleFilesDto &files)
{
    // Se limpia primero: un builder reutilizado duplicaría las imágenes.
    builder.clearFiles();
    for (const domain::VehicleImage &image : files.images)
        builder.addImage(image);
    for (const domain::VehicleDocument &document : files.documents) {
        if (!document.path.isEmpty())
            builder.addDocument(document);
    }
}

} // namespace application::mapping
