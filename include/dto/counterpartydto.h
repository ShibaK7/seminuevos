#ifndef COUNTERPARTYDTO_H
#define COUNTERPARTYDTO_H

#include <QDateTime>
#include <QList>
#include <QString>

// Contraparte (vendedor, propietario, comprador o aval). El rol es contextual
// en el flujo, no un tipo en BD. Sin lógica de negocio: solo carga útil.
//
// Catálogos no llevan DTO (Rule 1). Esta tabla sí: CRUD reutilizable (Rule 2)
// y alta anidada dentro de adquisición / consignación / venta (Rule 3).
struct CounterpartyDocumentDTO
{
    int id = 0;
    int counterpartyId = 0;
    QString documentType;
    QString filePath;
    QDateTime uploadedAt;
};

struct CounterpartyDTO
{
    int id = 0;
    QString fullName;
    QString nationalId;
    QString streetAddress;
    QString suburb;
    QString locality;
    QString state;
    QString postalCode;
    QString phone;
    QString email;
    QList<CounterpartyDocumentDTO> documents;
};

#endif // COUNTERPARTYDTO_H
