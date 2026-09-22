#include "services/counterpartyservice.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>

QList<CounterpartyLookupDTO> CounterpartyService::searchByName(const QString& query, int limit) {
    QString sanitizedQuery = query.trimmed();
    if (sanitizedQuery.length() < 2) {
        return QList<CounterpartyLookupDTO>();
    }
    return CounterpartyRepository::searchByName(sanitizedQuery, limit);
}

bool CounterpartyService::getCounterpartyById(int id, CounterpartyDTO& outDto, QString& errorMessage) {
    if (id <= 0) {
        errorMessage = "El identificador de la persona/contraparte no es válido.";
        return false;
    }

    if (!CounterpartyRepository::getCounterpartyById(id, outDto)) {
        errorMessage = QString("No se encontró ningún registro para el ID %1.").arg(id);
        return false;
    }

    return true;
}

bool CounterpartyService::validate(const CounterpartyDTO& dto, QString& errorMessage) {
    if (dto.fullName.trimmed().isEmpty()) {
        errorMessage = "El nombre completo o razón social es obligatorio.";
        return false;
    }

    if (dto.fullName.trimmed().length() < 3) {
        errorMessage = "El nombre completo debe tener al menos 3 caracteres.";
        return false;
    }

    if (!dto.email.trimmed().isEmpty()) {
        static const QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
        if (!emailRegex.match(dto.email.trimmed()).hasMatch()) {
            errorMessage = "El formato del correo electrónico ingresado no es válido.";
            return false;
        }
    }

    if (!dto.phone.trimmed().isEmpty()) {
        QString digitsOnly = dto.phone;
        digitsOnly.remove(QRegularExpression(R"(\D)")); // Remueve caracteres no numéricos
        if (digitsOnly.length() < 8 || digitsOnly.length() > 15) {
            errorMessage = "El número telefónico debe contener entre 8 y 15 dígitos.";
            return false;
        }
    }

    return true;
}

bool CounterpartyService::saveCounterparty(CounterpartyDTO& dto, int& outId, QString& errorMessage) {
    dto.fullName = dto.fullName.trimmed();
    dto.nationalId = dto.nationalId.trimmed().toUpper(); // Ej. INE/RFC en mayúsculas
    dto.email = dto.email.trimmed().toLower();
    dto.phone = dto.phone.trimmed();

    if (!validate(dto, errorMessage)) {
        return false;
    }

    if (!CounterpartyRepository::addCounterparty(dto, outId)) {
        errorMessage = "Ocurrió un error al guardar la información en la base de datos.";
        return false;
    }

    dto.id = outId;
    return true;
}