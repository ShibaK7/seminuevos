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
    
    q.prepare(R"(
        INSERT INTO vehicles (
            acquisition_type, status, vehicle_type_id, subtype_id, brand_id, model, year_model, color, mileage, serial_number, motor_number, plates, plates_holder, repuve, description
        ) VALUES (
            :acquisition_type, :status, :vehicle_type_id, :subtype_id, :brand_id, :model, :year_model, :color, :mileage, :serial_number, :motor_number, :plates, :plates_holder, :repuve, :description
        ) RETURNING id;
    )");

    q.bindValue(":brandId", dto.brandId);
    q.bindValue(":subtypeId", dto.subtypeId);
    q.bindValue(":year", dto.year);
    q.bindValue(":vin", dto.vin.isEmpty() ? QVariant(QVariant::String) : dto.vin);
    q.bindValue(":licensePlate", dto.licensePlate.isEmpty() ? QVariant(QVariant::String) : dto.licensePlate);
    q.bindValue(":color", dto.color.isEmpty() ? QVariant(QVariant::String) : dto.color);
    q.bindValue(":purchasePrice", dto.purchasePrice);
    q.bindValue(":ownerId", dto.currentOwnerId <= 0 ? QVariant(QVariant::Int) : dto.currentOwnerId);
    q.bindValue(":notes", dto.notes.isEmpty() ? QVariant(QVariant::String) : dto.notes);

    if (!q.exec() || !q.next()) {
        qCritical() << "VehicleRepository Error:" << q.lastError().text();
        db.rollback();
        return false;
    }

    outId = q.value(0).toInt();
    dto.id = outId;

    // Sincronización de documentos del vehículo
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

    if (!db.commit()) {
        db.rollback();
        return false;
    }

    return true;
}