#include "application/services/vehicleregistrationservice.h"

#include "domain/model/vehicle.h"
#include "domain/model/vehiclebuilder.h"
#include "domain/rules/uploadformatpolicy.h"
#include "registrationmapping.h"

#include <QStringList>

namespace application {

namespace {

const domain::UploadFormatPolicy &policyFor(UploadKind kind)
{
    return kind == UploadKind::Image ? domain::UploadFormatPolicy::images()
                                     : domain::UploadFormatPolicy::documents();
}

RegistrationResult rejected(const domain::ValidationResult &validation)
{
    RegistrationResult result;
    result.status = RegistrationResult::Status::Rejected;
    result.validation = validation;
    return result;
}

RegistrationResult failed(const QString &message)
{
    RegistrationResult result;
    result.status = RegistrationResult::Status::Failed;
    result.errorMessage = message;
    return result;
}

RegistrationResult duplicateSerialNumber()
{
    domain::ValidationResult validation;
    validation.addError(QStringLiteral("serialNumber"),
                        QStringLiteral("Ya hay una unidad registrada con ese número de serie (VIN)."));
    return rejected(validation);
}

} // namespace

VehicleRegistrationService::VehicleRegistrationService(VehicleRepository &vehicles, FileStorage &files,
                                                       ReferenceDataReader &referenceData,
                                                       ContractGenerator &contracts)
    : m_vehicles(vehicles)
    , m_files(files)
    , m_referenceData(referenceData)
    , m_contracts(contracts)
{
}

RegistrationLookupsDto VehicleRegistrationService::loadLookups() const
{
    RegistrationLookupsDto lookups;
    QStringList errors;
    QString error;

    const QList<CatalogOptionDto> categories = m_referenceData.vehicleCategories(&error);
    if (!error.isEmpty())
        errors << error;
    for (const CatalogOptionDto &option : categories) {
        if (option.parentId < 0)
            lookups.vehicleTypes << option;
        else
            lookups.vehicleSubtypes << option;
    }

    error.clear();
    lookups.brands = m_referenceData.brands(&error);
    if (!error.isEmpty())
        errors << error;

    error.clear();
    lookups.fuelTypes = m_referenceData.fuelTypes(&error);
    if (!error.isEmpty())
        errors << error;

    error.clear();
    lookups.checklist = m_referenceData.conditionChecklist(&error);
    if (!error.isEmpty())
        errors << error;

    error.clear();
    lookups.umaDailyValue = m_referenceData.umaDailyValue(&error);
    if (!error.isEmpty())
        errors << error;

    lookups.errorMessage = errors.join(QStringLiteral("\n"));
    return lookups;
}

domain::ValidationResult VehicleRegistrationService::validateDetails(
    const VehicleDetailsDto &details, std::optional<double> umaDailyValue) const
{
    domain::VehicleBuilder builder;
    domain::ValidationResult result = mapping::applyDetails(builder, details, umaDailyValue);
    result.merge(builder.validateVehicleData());
    return result;
}

domain::ValidationResult VehicleRegistrationService::validateConditions(
    const VehicleConditionsDto &conditions) const
{
    domain::VehicleBuilder builder;
    domain::ValidationResult result = mapping::applyConditions(builder, conditions);
    result.merge(builder.validateConditionData());
    return result;
}

RegistrationResult VehicleRegistrationService::registerVehicle(
    const VehicleRegistrationDto &registration) const
{
    // La UMA se lee al guardar, no se confía en la que tenía la pantalla.
    QString umaError;
    const std::optional<double> uma = m_referenceData.umaDailyValue(&umaError);
    if (!umaError.isEmpty())
        return failed(QStringLiteral("No se pudo leer la UMA: %1").arg(umaError));

    // 1. Validar sin tocar disco ni base.
    domain::VehicleBuilder builder;
    domain::ValidationResult mappingErrors = mapping::applyDetails(builder, registration.details, uma);
    mappingErrors.merge(mapping::applyConditions(builder, registration.conditions));
    mapping::applyFiles(builder, registration.files);

    domain::ValidationResult validation;
    std::unique_ptr<domain::Vehicle> vehicle = builder.build(validation);
    validation.merge(mappingErrors);
    if (!vehicle || !validation.isValid())
        return rejected(validation);

    // 2. Un VIN repetido lo rechazaría la base, pero ya con los archivos
    // copiados. Se pregunta antes.
    QString lookupError;
    if (m_vehicles.serialNumberExists(vehicle->serialNumber(), &lookupError))
        return duplicateSerialNumber();
    if (!lookupError.isEmpty())
        return failed(QStringLiteral("No se pudo revisar el número de serie: %1").arg(lookupError));

    // 3. Copiar los archivos, anotando cada uno para poder deshacerlo.
    QStringList stored;
    const auto rollbackFiles = [this, &stored] {
        for (const QString &path : std::as_const(stored))
            m_files.remove(path);
    };
    const QString key = vehicle->serialNumber();

    if (!vehicle->invoiceFilePath().isEmpty()) {
        const FileStorage::Stored copy =
            m_files.store(key, vehicle->invoiceFilePath(), FileStorage::Kind::Document);
        if (!copy.ok) {
            rollbackFiles();
            return failed(QStringLiteral("Error guardando la factura: %1").arg(copy.errorMessage));
        }
        stored << copy.relativePath;
        vehicle->setInvoiceFilePath(copy.relativePath);
    }
    for (int i = 0; i < vehicle->images().size(); ++i) {
        const FileStorage::Stored copy =
            m_files.store(key, vehicle->images().at(i).path, FileStorage::Kind::Image);
        if (!copy.ok) {
            rollbackFiles();
            return failed(QStringLiteral("Error guardando una fotografía: %1").arg(copy.errorMessage));
        }
        stored << copy.relativePath;
        (void)vehicle->setImageStoredPath(i, copy.relativePath);
    }
    for (int i = 0; i < vehicle->documents().size(); ++i) {
        const FileStorage::Stored copy =
            m_files.store(key, vehicle->documents().at(i).path, FileStorage::Kind::Document);
        if (!copy.ok) {
            rollbackFiles();
            return failed(QStringLiteral("Error guardando un documento: %1").arg(copy.errorMessage));
        }
        stored << copy.relativePath;
        (void)vehicle->setDocumentStoredPath(i, copy.relativePath);
    }

    // 4. Guardar todo o nada.
    const VehicleRepository::SaveOutcome outcome = m_vehicles.add(*vehicle);
    if (!outcome.ok) {
        rollbackFiles();
        if (outcome.duplicateSerialNumber)
            return duplicateSerialNumber();
        return failed(outcome.errorMessage);
    }

    RegistrationResult result;
    result.status = RegistrationResult::Status::Registered;
    result.folio = outcome.folio;
    result.contract = vehicle->contractData();
    return result;
}

ContractGenerator::Outcome VehicleRegistrationService::generateContract(
    const domain::ContractData &contract, const QString &outputPath) const
{
    return m_contracts.generate(contract, outputPath);
}

UploadFormatsDto VehicleRegistrationService::uploadFormats(UploadKind kind) const
{
    const domain::UploadFormatPolicy &policy = policyFor(kind);
    return {policy.dialogFilter(), policy.describeFormats(), policy.extensions()};
}

UploadCheckDto VehicleRegistrationService::checkUpload(const QString &path, UploadKind kind) const
{
    const domain::FileFacts facts = m_files.inspect(path);
    UploadCheckDto check;
    check.fileName = facts.fileName;
    check.accepted = policyFor(kind).accepts(facts, &check.reason);
    return check;
}

QByteArray VehicleRegistrationService::filePreview(const QString &path) const
{
    return m_files.read(path);
}

TemporaryFileDto VehicleRegistrationService::prepareCfdiRequestForm() const
{
    // La plantilla viaja dentro del ejecutable (resources.qrc).
    return m_files.copyToTemporary(QStringLiteral(":/templates/request_issuance_cfdi.html"),
                                   QStringLiteral("request_issuance_cfdi.html"));
}

} // namespace application
