#ifndef VEHICLEDTO_H
#define VEHICLEDTO_H

#include <QDate>
#include <QList>
#include <QString>

// Núcleo de `vehicles` + condiciones, inspección y archivos. Lo reutilizan
// AcquisitionDTO y ConsignationDTO. Las grillas de inventario no usan este
// struct: van con QSqlQueryModel (Rule 1).
struct InspectionItemDTO
{
    int vehicleId = 0;
    int elementId = -1;
    bool isOptimal = true;
    QString observations;
};

struct VehicleImageDTO
{
    int vehicleId = 0;
    QString filePath;
    bool isPrimary = false;
};

struct VehicleDocumentDTO
{
    int vehicleId = 0;
    QString documentType;
    QString filePath;
    QString documentNumber;
    bool isVerified = false;
};

struct VehicleMaintenanceDTO
{
    int vehicleId = 0;
    QDate maintenanceDate;
    int maintenanceTypeId = -1;
    double cost = 0.0;
    QString description;
};

struct VehicleDTO
{
    int folio = 0;
    QString acquisitionType;
    QString status = QStringLiteral("Disponible");

    int vehicleTypeId = -1;
    int subtypeId = -1;
    int brandId = -1;
    QString model;
    int yearModel = 0;
    QString color;
    int mileage = 0;
    QString serialNumber;
    QString motorNumber;
    QString plates;
    QString platesHolder;
    QString repuve;
    QString description;
    QDate addedDate;

    int fuelTypeId = -1;
    int cylinders = 0;
    QString transmission;
    QString interiorMaterial;
    QString windowRegulators;
    QString airConditioning;

    QList<InspectionItemDTO> inspection;
    QList<VehicleImageDTO> images;
    QList<VehicleDocumentDTO> documents;
};

#endif // VEHICLEDTO_H
