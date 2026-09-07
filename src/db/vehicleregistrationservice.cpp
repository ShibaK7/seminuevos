#include "../../include/db/vehicleregistrationservice.h"
#include "../../include/vehiclewizard/vehicledraft.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QVariant nullableInt(const QString &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return ok ? QVariant(parsed) : QVariant(QMetaType(QMetaType::Int));
}

// Busca al vendedor por identificación (si la trae); si no existe, lo crea.
// Devuelve el id o -1 si algo falla (errorMessage queda lleno).
int findOrCreateSeller(QSqlDatabase &db, const OwnerDraft &owner, QString &errorMessage)
{
    if (!owner.nationalId.trimmed().isEmpty()) {
        QSqlQuery findQuery(db);
        findQuery.prepare(QStringLiteral(
            "SELECT id FROM counterparties WHERE national_id = :national_id LIMIT 1"));
        findQuery.bindValue(QStringLiteral(":national_id"), owner.nationalId);
        if (!findQuery.exec()) {
            errorMessage = findQuery.lastError().text();
            return -1;
        }
        if (findQuery.next())
            return findQuery.value(0).toInt();
    }

    QSqlQuery insertQuery(db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO counterparties "
        "(full_name, national_id, street_address, suburb, locality, state, postal_code) "
        "VALUES (:full_name, :national_id, :street_address, :suburb, :locality, :state, :postal_code) "
        "RETURNING id"));
    insertQuery.bindValue(QStringLiteral(":full_name"), owner.fullName);
    insertQuery.bindValue(QStringLiteral(":national_id"),
                           owner.nationalId.trimmed().isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : owner.nationalId);
    insertQuery.bindValue(QStringLiteral(":street_address"), owner.streetAddress);
    insertQuery.bindValue(QStringLiteral(":suburb"), owner.suburb);
    insertQuery.bindValue(QStringLiteral(":locality"), owner.locality);
    insertQuery.bindValue(QStringLiteral(":state"), owner.state);
    insertQuery.bindValue(QStringLiteral(":postal_code"), owner.postalCode);

    if (!insertQuery.exec() || !insertQuery.next()) {
        errorMessage = insertQuery.lastError().text();
        return -1;
    }
    return insertQuery.value(0).toInt();
}

int insertVehicleRow(QSqlDatabase &db, const VehicleDraft &draft, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicles "
        "(acquisition_type, vehicle_type_id, subtype_id, brand_id, model, year_model, color, "
        " mileage, serial_number, motor_number, plates, plates_holder, repuve, description, added_date) "
        "VALUES ('Adquisición', :vehicle_type_id, :subtype_id, :brand_id, :model, :year_model, :color, "
        " :mileage, :serial_number, :motor_number, :plates, :plates_holder, :repuve, :description, :added_date) "
        "RETURNING folio"));
    query.bindValue(QStringLiteral(":vehicle_type_id"), nullableInt(draft.vehicleTypeId));
    query.bindValue(QStringLiteral(":subtype_id"), nullableInt(draft.subtypeId));
    query.bindValue(QStringLiteral(":brand_id"), nullableInt(draft.brandId));
    query.bindValue(QStringLiteral(":model"), draft.model);
    query.bindValue(QStringLiteral(":year_model"), draft.yearModel);
    query.bindValue(QStringLiteral(":color"), draft.color);
    query.bindValue(QStringLiteral(":mileage"), draft.mileage);
    query.bindValue(QStringLiteral(":serial_number"), draft.serialNumber);
    query.bindValue(QStringLiteral(":motor_number"), draft.motorNumber);
    query.bindValue(QStringLiteral(":plates"), draft.plates);
    query.bindValue(QStringLiteral(":plates_holder"), draft.platesHolder);
    query.bindValue(QStringLiteral(":repuve"), draft.repuve);
    query.bindValue(QStringLiteral(":description"), draft.description);
    query.bindValue(QStringLiteral(":added_date"), draft.date);

    if (!query.exec() || !query.next()) {
        errorMessage = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}

bool insertAcquisition(QSqlDatabase &db, int folio, int sellerId, const VehicleDraft &draft, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_acquisitions "
        "(vehicle_folio, seller_id, invoice_type, invoice_number, invoice_issuer, invoice_file_path, "
        " purchase_price, payment_type, payment_method, maintenance_cost, sale_price, observations, acquisition_date) "
        "VALUES (:vehicle_folio, :seller_id, :invoice_type, :invoice_number, :invoice_issuer, :invoice_file_path, "
        " :purchase_price, :payment_type, :payment_method, :maintenance_cost, :sale_price, :observations, :acquisition_date)"));
    query.bindValue(QStringLiteral(":vehicle_folio"), folio);
    query.bindValue(QStringLiteral(":seller_id"), sellerId);
    query.bindValue(QStringLiteral(":invoice_type"), draft.invoiceType);
    query.bindValue(QStringLiteral(":invoice_number"), draft.invoiceNumber);
    query.bindValue(QStringLiteral(":invoice_issuer"), draft.invoiceIssuer);
    query.bindValue(QStringLiteral(":invoice_file_path"), draft.invoiceFilePath);
    query.bindValue(QStringLiteral(":purchase_price"), draft.purchasePrice);
    query.bindValue(QStringLiteral(":payment_type"), draft.paymentType);
    query.bindValue(QStringLiteral(":payment_method"), draft.paymentMethod);
    query.bindValue(QStringLiteral(":maintenance_cost"), draft.maintenanceCost);
    query.bindValue(QStringLiteral(":sale_price"), draft.salePrice);
    query.bindValue(QStringLiteral(":observations"), draft.observations);
    query.bindValue(QStringLiteral(":acquisition_date"), draft.date);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool insertConditions(QSqlDatabase &db, int folio, const VehicleDraft &draft, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_conditions "
        "(vehicle_folio, fuel_type_id, cylinders, transmission, interior_material, window_regulators, air_conditioning) "
        "VALUES (:vehicle_folio, :fuel_type_id, :cylinders, :transmission, :interior_material, :window_regulators, :air_conditioning)"));
    query.bindValue(QStringLiteral(":vehicle_folio"), folio);
    query.bindValue(QStringLiteral(":fuel_type_id"), nullableInt(draft.fuelTypeId));
    query.bindValue(QStringLiteral(":cylinders"), draft.cylinders);
    query.bindValue(QStringLiteral(":transmission"), draft.transmission);
    query.bindValue(QStringLiteral(":interior_material"), draft.interiorMaterial);
    query.bindValue(QStringLiteral(":window_regulators"), draft.windowRegulators);
    query.bindValue(QStringLiteral(":air_conditioning"), draft.airConditioning);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool insertConditionItems(QSqlDatabase &db, int folio, const VehicleDraft &draft, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_condition_items (vehicle_folio, item_key, item_group, is_checked, status, observations) "
        "VALUES (:vehicle_folio, :item_key, :item_group, :is_checked, :status, :observations)"));

    for (const ConditionItemValue &item : draft.conditionItems) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":item_key"), item.itemKey);
        query.bindValue(QStringLiteral(":item_group"), item.itemGroup);
        query.bindValue(QStringLiteral(":is_checked"), item.isChecked);
        query.bindValue(QStringLiteral(":status"), item.status);
        query.bindValue(QStringLiteral(":observations"), item.observations);
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool insertImages(QSqlDatabase &db, int folio, const VehicleDraft &draft, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_images (vehicle_folio, file_path, is_primary) "
        "VALUES (:vehicle_folio, :file_path, :is_primary)"));

    for (const PendingImage &image : draft.images) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":file_path"), image.sourcePath);
        query.bindValue(QStringLiteral(":is_primary"), image.isPrimary);
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool insertDocuments(QSqlDatabase &db, int folio, const VehicleDraft &draft, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_documents (vehicle_folio, document_type, file_path, document_number) "
        "VALUES (:vehicle_folio, :document_type, :file_path, :document_number)"));

    for (const PendingDocument &document : draft.documents) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":document_type"), document.documentType);
        query.bindValue(QStringLiteral(":file_path"), document.sourcePath);
        query.bindValue(QStringLiteral(":document_number"), document.documentNumber);
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

VehicleRegistrationService::Result VehicleRegistrationService::insertVehicle(QSqlDatabase &db, const VehicleDraft &draft)
{
    Result result;

    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1").arg(db.lastError().text());
        return result;
    }

    QString errorMessage;

    const int sellerId = findOrCreateSeller(db, draft.owner, errorMessage);
    if (sellerId < 0) {
        result.errorMessage = QStringLiteral("Error registrando al propietario anterior: %1").arg(errorMessage);
        db.rollback();
        return result;
    }

    const int folio = insertVehicleRow(db, draft, errorMessage);
    if (folio < 0) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1").arg(errorMessage);
        db.rollback();
        return result;
    }

    if (!insertAcquisition(db, folio, sellerId, draft, errorMessage)
        || !insertConditions(db, folio, draft, errorMessage)
        || !insertConditionItems(db, folio, draft, errorMessage)
        || !insertImages(db, folio, draft, errorMessage)
        || !insertDocuments(db, folio, draft, errorMessage)) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1").arg(errorMessage);
        db.rollback();
        return result;
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1").arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    result.folio = folio;
    return result;
}
