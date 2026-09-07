#ifndef DOMAIN_VEHICLEIMAGE_H
#define DOMAIN_VEHICLEIMAGE_H

#include <QString>

namespace domain {

// Una fotografía del vehículo. Mapea un renglón de vehicle_images.
//
// Se queda como struct con campos públicos y sin getters: su única regla
// ("la ruta no está vacía") no da para encapsular nada, y envolver dos
// campos en cuatro métodos sería relleno. Lo que sí tiene invariante es la
// COLECCIÓN -- a lo más una portada por vehículo -- y esa vive en Vehicle,
// que es quien la posee.
//
// `path` cambia de significado a lo largo del guardado: absoluta mientras la
// foto sigue donde el usuario la eligió, y relativa a la raíz de
// almacenamiento una vez copiada. Quien la reescribe es el worker, a través
// de Vehicle::setImageStoredPath().
struct VehicleImage
{
    QString path;
    bool isPrimary = false;
};

} // namespace domain

#endif // DOMAIN_VEHICLEIMAGE_H
