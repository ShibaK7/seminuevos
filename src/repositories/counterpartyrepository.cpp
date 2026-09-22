#include "counterpartyrepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

QList<CounterpartyLookupDTO> CounterpartyRepository::searchByName(const QString& nameQuery, int limit) {
    QList<CounterpartyLookupDTO> results;
    if (nameQuery.trimmed().isEmpty()) return results;

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());

    query.prepare("SELECT id, full_name, national_id FROM counterparties WHERE full_name ILIKE :query ORDER BY full_name ASC LIMIT :limit;");
    query.bindValue(":query", "%" + nameQuery.trimmed() + "%");
    query.bindValue(":limit", limit);

    if (query.exec()) {
        while (query.next()) {
            CounterpartyLookupDTO item;
            item.id = query.value("id").toInt();
            item.fullName = query.value("full_name").toString();
            item.nationalId = query.value("national_id").toString();
            results.append(item);
        }
    } else {
        qCritical() << "CounterpartyRepository::searchByName Error:" << query.lastError().text();
    }

    return results;
}

bool CounterpartyRepository::getCounterpartyById(int id, CounterpartyDTO& outDto) {
    if (id <= 0) return false;

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());

    query.prepare("SELECT id, full_name, national_id, street_address, suburb, locality, state, postal_code, phone, email FROM counterparties WHERE id = :id;");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        outDto.id = query.value("id").toInt();
        outDto.fullName = query.value("full_name").toString();
        outDto.nationalId = query.value("national_id").toString();
        outDto.streetAddress = query.value("street_address").toString();
        outDto.suburb = query.value("suburb").toString();
        outDto.locality = query.value("locality").toString();
        outDto.state = query.value("state").toString();
        outDto.postalCode = query.value("postal_code").toString();
        outDto.phone = query.value("phone").toString();
        outDto.email = query.value("email").toString();
        
        QSqlQuery qDocs(handle.database());
        qDocs.prepare("SELECT id, counterparty_id, document_type, file_path, uploaded_at FROM counterparty_documents WHERE counterparty_id = :counterpartyId;");
        qDocs.bindValue(":counterpartyId", outDto.id);

        outDto.documents.clear();
        if (qDocs.exec()) {
            while (qDocs.next()) {
                CounterpartyDocumentDTO doc;
                doc.id = qDocs.value("id").toInt();
                doc.counterpartyId = qDocs.value("counterparty_id").toInt();
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

bool CounterpartyRepository::addCounterparty(const CounterpartyDTO& dto, int& outId) {
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase db = handle.database();

    if (!db.transaction()) {
        qCritical() << "CounterpartyRepository: No se pudo iniciar la transacción.";
        return false;
    }

    QSqlQuery query(db);

    query.prepare(R"(
        INSERT INTO counterparties (
            full_name, national_id, street_address, suburb, locality, state, postal_code, phone, email
        ) VALUES (
            :fullName, :nationalId, :streetAddress, :suburb, :locality, :state, :postalCode, :phone, :email
        ) RETURNING id;
    )");

    query.bindValue(":fullName", dto.fullName);
    query.bindValue(":nationalId", dto.nationalId.isEmpty() ? QVariant(QVariant::String) : dto.nationalId);
    query.bindValue(":streetAddress", dto.streetAddress.isEmpty() ? QVariant(QVariant::String) : dto.streetAddress);
    query.bindValue(":suburb", dto.suburb.isEmpty() ? QVariant(QVariant::String) : dto.suburb);
    query.bindValue(":locality", dto.locality.isEmpty() ? QVariant(QVariant::String) : dto.locality);
    query.bindValue(":state", dto.state.isEmpty() ? QVariant(QVariant::String) : dto.state);
    query.bindValue(":postalCode", dto.postalCode.isEmpty() ? QVariant(QVariant::String) : dto.postalCode);
    query.bindValue(":phone", dto.phone.isEmpty() ? QVariant(QVariant::String) : dto.phone);
    query.bindValue(":email", dto.email.isEmpty() ? QVariant(QVariant::String) : dto.email);

    if (!q.exec() || !q.next()) {
        qCritical() << "Error insertando persona base:" << q.lastError().text();
        db.rollback(); // Reversión total si falla la persona
        return false;
    }

    outId = q.value(0).toInt();
    dto.id = outId;

    for (auto& doc : dto.documents) {
        doc.counterpartyId = outId; // Asignamos la FK
        
        QSqlQuery qDoc(db);
        qDoc.prepare(R"(
            INSERT INTO counterparty_documents (counterparty_id, document_type, file_path)
            VALUES (:counterpartyId, :docType, :filePath)
            ON CONFLICT (counterparty_id, document_type) 
            DO UPDATE SET file_path = EXCLUDED.file_path, uploaded_at = CURRENT_TIMESTAMP
            RETURNING id;
        )");

        qDoc.bindValue(":counterpartyId", doc.counterpartyId);
        qDoc.bindValue(":docType", doc.documentType);
        qDoc.bindValue(":filePath", doc.filePath);

        if (!qDoc.exec() || !qDoc.next()) {
            qCritical() << "Error insertando documento para persona ID" << outId << ":" << qDoc.lastError().text();
            db.rollback(); // Reversión total si falla un solo documento
            return false;
        }

        doc.id = qDoc.value(0).toInt();
    }

    if (!db.commit()) {
        qCritical() << "Error ejecutando COMMIT en addCounterparty:" << db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}