#ifndef VEHICLEDRAFT_H
#define VEHICLEDRAFT_H

#include <QDate>
#include <QList>
#include <QString>

// Datos en memoria recolectados por los 3 pasos del wizard antes del commit
// atómico final. Ningún dato aquí toca disco/BD hasta que Step3 confirma.

struct OwnerDraft
{
    QString fullName;
    QString nationalId;
    QString streetAddress;
    QString suburb;
    QString locality;
    QString state;
    QString postalCode;
};

struct ConditionItemValue
{
    QString itemKey;
    QString itemGroup;
    bool isChecked = true;
    // "Estado óptimo" o "Con fallas" (valor canónico guardado en BD; la UI
    // puede mostrar una etiqueta distinta para el estado negativo).
    QString status = QStringLiteral("Estado óptimo");
    QString observations;
};

struct PendingImage
{
    QString sourcePath;
    bool isPrimary = false;
};

struct PendingDocument
{
    QString documentType;
    QString sourcePath;
    QString documentNumber;
};

struct VehicleDraft
{
    // --- Paso 1: Detalles ---
    QDate date = QDate::currentDate();
    QString vehicleTypeId;
    QString subtypeId;
    QString brandId;
    QString brandName;
    QString model;
    int yearModel = 0;
    QString color;
    int mileage = 0;
    QString description;
    QString motorNumber;
    QString serialNumber; // VIN
    QString repuve;
    QString plates;
    QString platesHolder;

    OwnerDraft owner;

    QString invoiceType; // "Facturado" | "Autofactura"
    QString invoiceNumber;
    QString invoiceIssuer;
    QString invoiceFilePath;

    double purchasePrice = 0.0;
    QString paymentType;   // "Contado" | "Crédito"
    QString paymentMethod; // "Efectivo" | "Transferencia"
    double maintenanceCost = 0.0;
    double salePrice = 0.0;
    QString observations;

    // --- Paso 2: Condición ---
    QString fuelTypeId;
    int cylinders = 0;
    QString transmission;      // "Automático" | "Manual"
    QString interiorMaterial;
    QString windowRegulators;
    QString airConditioning;   // "Automático" | "Manual"
    QList<ConditionItemValue> conditionItems;

    // --- Paso 3: Archivos ---
    QList<PendingImage> images;
    QList<PendingDocument> documents;
};

#endif // VEHICLEDRAFT_H
