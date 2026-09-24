#include "repositories/vehiclerepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

bool VehicleRepository::getVehicleById(int id, VehicleDTO& outDto) {
    if (id <= 0) return false;

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery q(handle.database());

    q.prepare(R"(
        SELECT folio, brand_id, subtype_id, year, vin, license_plate, color, purchase_price, current_owner_id, notes
        FROM vehicles WHERE id = :id;
    )");
    q.bindValue(":id", id);

    if (q.exec() && q.next()) {
        outDto.id = q.value("id").toInt();
        outDto.brandId = q.value("brand_id").toInt();
        outDto.subtypeId = q.value("subtype_id").toInt();
        outDto.year = q.value("year").toInt();
        outDto.vin = q.value("vin").toString();
        outDto.licensePlate = q.value("license_plate").toString();
        outDto.color = q.value("color").toString();
        outDto.purchasePrice = q.value("purchase_price").toDouble();
        outDto.currentOwnerId = q.value("current_owner_id").toInt();
        outDto.notes = q.value("notes").toString();

        // Carga de documentos del vehículo
        QSqlQuery qDocs(handle.database());
        qDocs.prepare("SELECT id, vehicle_id, document_type, file_path, uploaded_at FROM vehicle_documents WHERE vehicle_id = :vehicleId;");
        qDocs.bindValue(":vehicleId", outDto.id);

        outDto.documents.clear();
        if (qDocs.exec()) {
            while (qDocs.next()) {
                VehicleDocumentDTO doc;
                doc.id = qDocs.value("id").toInt();
                doc.vehicleId = qDocs.value("vehicle_id").toInt();
                doc.documentType = qDocs.value("document_type").toString();
                doc.filePath = qDocs.value("file_path").toString();
                doc.uploadedAt = qDocs.value("uploaded_at").toDateTime();
                outDto.documents.append(doc);
            }
        }
        return true;
    }
    return false;
}

bool VehicleRepository::save(VehicleDTO& dto, int& outId) {
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase db = handle.database();

    if (!db.transaction()) {
        qCritical() << "VehicleRepository: Error al iniciar transacción SQL.";
        return false;
    }

    QSqlQuery q(db);
    
    if (dto.id <= 0) {
        q.prepare(R"(
            INSERT INTO vehicles (
                acquisition_type, status, vehicle_type_id, subtype_id, brand_id, model,
                year_model, color, mileage, serial_number, motor_number, plates,
                plates_holder, repuve, description
            ) VALUES (
                :acquisition_type, :status, :vehicle_type_id, :subtype_id, :brand_id, :model,
                :year_model, :color, :mileage, :serial_number, :motor_number, :plates,
                :plates_holder, :repuve, :description
            ) RETURNING id;
        )");
    } else {
        q.prepare(R"(
            UPDATE vehicles SET
                acquisition_type = :acquisition_type, status = :status, vehicle_type_id = :vehicle_type_id,
                subtype_id = :subtype_id, brand_id = :brand_id, model = :model, year_model = :year_model,
                color = :color, mileage = :mileage, serial_number = :serial_number, motor_number = :motor_number,
                plates = :plates, plates_holder = :plates_holder, repuve = :repuve, description = :description,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = :id RETURNING id;
        )");
        q.bindValue(":id", dto.id);
    }

    q.bindValue(":acquisition_type", dto.acquisitionType.isEmpty() ? QVariant(QVariant::String) : dto.acquisitionType);
    q.bindValue(":status", dto.status.isEmpty() ? QVariant(QVariant::String) : dto.status);
    q.bindValue(":vehicle_type_id", dto.vehicleTypeId <= 0 ? QVariant(QVariant::Int) : dto.vehicleTypeId);
    q.bindValue(":subtype_id", dto.subtypeId <= 0 ? QVariant(QVariant::Int) : dto.subtypeId);
    q.bindValue(":brand_id", dto.brandId <= 0 ? QVariant(QVariant::Int) : dto.brandId);
    q.bindValue(":model", dto.model.isEmpty() ? QVariant(QVariant::String) : dto.model);
    q.bindValue(":year_model", dto.yearModel <= 0 ? QVariant(QVariant::Int) : dto.yearModel);
    q.bindValue(":color", dto.color.isEmpty() ? QVariant(QVariant::String) : dto.color);
    q.bindValue(":mileage", dto.mileage);
    q.bindValue(":serial_number", dto.serialNumber.isEmpty() ? QVariant(QVariant::String) : dto.serialNumber);
    q.bindValue(":motor_number", dto.motorNumber.isEmpty() ? QVariant(QVariant::String) : dto.motorNumber);
    q.bindValue(":plates", dto.plates.isEmpty() ? QVariant(QVariant::String) : dto.plates);
    q.bindValue(":plates_holder", dto.platesHolder.isEmpty() ? QVariant(QVariant::String) : dto.platesHolder);
    q.bindValue(":repuve", dto.repuve.isEmpty() ? QVariant(QVariant::String) : dto.repuve);
    q.bindValue(":description", dto.description.isEmpty() ? QVariant(QVariant::String) : dto.description);

    if (!q.exec() || !q.next()) {
        qCritical() << "VehicleRepository Error:" << q.lastError().text();
        db.rollback();
        return false;
    }

    outId = q.value(0).toInt();
    dto.id = outId;

    for (auto& doc : dto.documents) {
        doc.vehicleId = outId;

        QSqlQuery qDoc(db);
        qDoc.prepare(R"(
            INSERT INTO vehicle_documents (vehicle_id, document_type, file_path)
            VALUES (:vehicleId, :docType, :filePath)
            ON CONFLICT (vehicle_id, document_type)
            DO UPDATE SET file_path = EXCLUDED.file_path, uploaded_at = CURRENT_TIMESTAMP
            RETURNING id;
        )");

        qDoc.bindValue(":vehicleId", doc.vehicleId);
        qDoc.bindValue(":docType", doc.documentType);
        qDoc.bindValue(":filePath", doc.filePath);

        if (!qDoc.exec() || !qDoc.next()) {
            qCritical() << "VehicleRepository Error guardando documento:" << qDoc.lastError().text();
            db.rollback();
            return false;
        }

        doc.id = qDoc.value(0).toInt();
    }

    // TODO: Complete other table information required

    if (!db.commit()) {
        db.rollback();
        return false;
    }

    return true;
}