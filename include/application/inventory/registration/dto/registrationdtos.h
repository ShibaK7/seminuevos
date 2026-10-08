#ifndef APPLICATION_INVENTORY_REGISTRATION_DTO_REGISTRATIONDTOS_H
#define APPLICATION_INVENTORY_REGISTRATION_DTO_REGISTRATIONDTOS_H

#include "domain/common/value_objects/catalogref.h"
#include "domain/inventory/value_objects/contractdata.h"
#include "domain/common/value_objects/enums.h"
#include "domain/common/value_objects/validationresult.h"
#include "domain/inventory/value_objects/vehicledocument.h"
#include "domain/inventory/value_objects/vehicleimage.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace application {

// DTOs del caso de uso "registrar vehículo". Uno por paso del asistente: cada
// paso se valida con lo suyo, y VehicleRegistrationDto los COMPONE en vez de
// repetir campos. No hay un VehicleDto genérico a propósito: copiar los ~30
// campos de Vehicle en un struct sería una segunda versión de la entidad sin
// sus reglas.
//
// Son structs planos, sin lógica. "Sin elegir" se expresa con std::optional:
// así un combo vacío no se convierte en el primer valor del enum.

struct CounterpartyDto
{
    QString fullName;
    QString nationalId;
    QString streetAddress;
    QString suburb;
    QString locality;
    QString state;
    QString postalCode;
};

// Paso 1: datos de la unidad y de la operación.
struct VehicleDetailsDto
{
    domain::AcquisitionType acquisitionType = domain::AcquisitionType::Adquisicion;
    QDate dealDate;
    domain::CatalogRef vehicleType;
    domain::CatalogRef subtype;
    domain::CatalogRef brand;
    QString model;
    int yearModel = 0;
    QString color;
    int mileage = 0;
    QString description;
    QString motorNumber;
    QString serialNumber;
    QString repuve;
    QString plates;
    QString platesHolder;
    CounterpartyDto counterparty;
    std::optional<domain::InvoiceType> invoiceType;
    QString invoiceNumber;
    QString invoiceIssuer;
    QString invoiceFilePath; // ruta de origen, la elige el usuario
    double maintenanceCost = 0.0;
    QString observations;
    // Compra
    double purchasePrice = 0.0;
    double salePrice = 0.0;
    domain::PaymentType paymentType = domain::PaymentType::Contado;
    domain::PaymentMethod paymentMethod = domain::PaymentMethod::Transferencia;
    // Consignación
    double basePrice = 0.0;
    double commissionRate = 0.0;
};

// Un renglón marcado del checklist.
struct InspectionEntryDto
{
    int elementId = -1;
    bool isOptimal = true;
    QString observations;
};

// Paso 2: especificaciones y checklist.
struct VehicleConditionsDto
{
    domain::CatalogRef fuelType;
    std::optional<int> cylinders;
    std::optional<domain::Transmission> transmission;
    QString interiorMaterial;
    std::optional<domain::WindowRegulators> windowRegulators;
    std::optional<domain::AirConditioning> airConditioning;
    QList<InspectionEntryDto> inspection; // solo las filas marcadas
};

// Paso 3: fotos y documentos, con su ruta de origen.
struct VehicleFilesDto
{
    QList<domain::VehicleImage> images;
    QList<domain::VehicleDocument> documents;
};

struct VehicleRegistrationDto
{
    VehicleDetailsDto details;
    VehicleConditionsDto conditions;
    VehicleFilesDto files;
};

struct RegistrationResult
{
    enum class Status {
        Registered, // quedó guardado
        Rejected,   // los datos no pasan las reglas (o el VIN ya existe)
        Failed,     // falló el disco o la base; se puede reintentar
    };
    Status status = Status::Failed;
    int folio = -1;
    domain::ValidationResult validation; // en Rejected, qué campo y por qué
    QString errorMessage;                // en Failed
    std::optional<domain::ContractData> contract; // en Registered, si aplica
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_DTO_REGISTRATIONDTOS_H
