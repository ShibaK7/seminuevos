#ifndef DOMAIN_VEHICLEDOCUMENT_H
#define DOMAIN_VEHICLEDOCUMENT_H

#include <QString>

namespace domain {

// Un documento adjunto al vehículo (tarjeta de circulación, reporte de no
// robo, tenencias). Mapea un renglón de vehicle_documents.
//
// Struct sin getters por la misma razón que VehicleImage: no tiene reglas
// propias. La invariante de la colección -- un solo documento por tipo, que
// es el UNIQUE (vehicle_folio, document_type) del esquema -- la aplica
// Vehicle::addDocument().
//
// `path` es absoluta hasta que el worker copia el archivo y la reemplaza por
// la relativa, vía Vehicle::setDocumentStoredPath().
struct VehicleDocument
{
    QString documentType;
    QString path;
    QString documentNumber;
};

} // namespace domain

#endif // DOMAIN_VEHICLEDOCUMENT_H
