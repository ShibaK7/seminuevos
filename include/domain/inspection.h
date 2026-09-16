#ifndef DOMAIN_INSPECTION_H
#define DOMAIN_INSPECTION_H

#include "domain/inspectionitem.h"
#include "domain/validationresult.h"

#include <QList>

namespace domain {

// El checklist de condición de UN vehículo: SOLO los elementos que la unidad
// trae, uno por renglón de vehicle_inspection. Lo que la unidad no trae se
// nota por ausencia, igual que en la tabla. Para obtener la lista de lo que
// falta hay que cruzar contra el catálogo (VehicleInspectionQuery), porque
// esta clase por sí sola no sabe qué elementos existen.
//
// Es una clase y no un QList<InspectionItem> suelto porque tiene una
// invariante propia -- no puede haber dos renglones para el mismo elemento,
// que es el UNIQUE (vehicle_folio, element_id) del esquema aplicado en
// memoria -- y consultas agregadas que solo tienen sentido sobre el conjunto
// (cuántas fallas, si la unidad está impecable). Ese es el criterio para
// decidir si algo merece ser clase: si no tuviera ni invariantes ni
// comportamiento, sería una lista y ya.
//
// Aplicar la unicidad aquí y no dejársela a PostgreSQL importa: el guardado
// ocurre en un hilo aparte y dentro de una transacción de seis tablas, así
// que un choque de llave llegaría al usuario como "duplicate key value
// violates unique constraint" después de haber copiado los archivos a disco.
class Inspection
{
public:
    Inspection() = default;

    // false si el elemento ya está en la lista o si el id es inválido.
    [[nodiscard]] bool addItem(const InspectionItem &item);

    // Agrega o reemplaza el renglón de ese elemento. Es lo que conviene usar
    // al volcar la pantalla completa: idempotente, así que volver a guardar
    // no duplica nada.
    void setItem(const InspectionItem &item);

    bool removeItem(int elementId);
    void clear();

    const QList<InspectionItem> &items() const;
    bool isEmpty() const;
    int itemCount() const;

    // Elementos que la unidad trae y están en mal estado.
    int faultCount() const;
    bool isFlawless() const;
    // Proporción de los elementos que la unidad trae que están en buen
    // estado, 0.0 a 1.0. Devuelve 0.0 con la lista vacía, para no dividir
    // entre cero.
    double optimalRatio() const;
    QList<int> faultyElementIds() const;

    ValidationResult validate() const;

private:
    // Se conserva el orden de captura, que es el orden del catálogo. La
    // unicidad se mantiene a mano en vez de con un QHash porque son unas
    // decenas de renglones y el orden importa para mostrarlos.
    QList<InspectionItem> m_items;
};

} // namespace domain

#endif // DOMAIN_INSPECTION_H
