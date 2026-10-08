#ifndef DOMAIN_COMMON_VALUE_OBJECTS_CATALOGREF_H
#define DOMAIN_COMMON_VALUE_OBJECTS_CATALOGREF_H

#include <QString>

namespace domain {

// Referencia a un renglón de una tabla de catálogo (marcas, tipos de
// vehículo, combustibles). Guarda el id que va a la llave foránea y el
// nombre que ya se leyó, para no tener que volver a la base solo para
// mostrar una etiqueta.
//
// El id arranca en -1 y no en 0 a propósito. Los combos del wizard pueden
// estar vacíos legítimamente mientras el usuario captura (el de subtipo, por
// ejemplo, se vacía cuando cambia el tipo padre), y ese estado tiene que
// llegar a la base como NULL. Con 0 como valor por omisión, un combo vacío
// se insertaría como id 0 y produciría una violación de llave foránea en vez
// del NULL que corresponde. isValid() es la pregunta que hay que hacerle
// antes de bindear.
struct CatalogRef
{
    int id = -1;
    QString name;

    bool isValid() const { return id > 0; }
};

} // namespace domain

#endif // DOMAIN_COMMON_VALUE_OBJECTS_CATALOGREF_H
