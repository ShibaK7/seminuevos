#ifndef DOMAIN_INSPECTIONITEM_H
#define DOMAIN_INSPECTIONITEM_H

#include "domain/validationresult.h"

#include <QString>

namespace domain {

// El resultado de revisar UN elemento del checklist en un vehículo. Mapea un
// renglón de vehicle_inspection.
//
// EXISTIR es la afirmación: si hay un InspectionItem para un elemento, la
// unidad lo trae. No hay un isChecked, y quitarlo no perdió información: un
// renglón marcado como "no lo trae" y la ausencia del renglón decían lo
// mismo, y mantener las dos formas obligaba a que cada consulta contemplara
// ambas. Los elementos que la unidad NO trae no viven aquí; se pueden obtener
// cruzando el catálogo (vehicle_conditions_cat) contra esta tabla con un LEFT
// JOIN. La condición del vehículo va en el ON y no en el WHERE: en el WHERE
// el LEFT JOIN se degrada a INNER y desaparecen justo las filas que faltan.
//
// isOptimal, entonces, ya no tiene un estado en el que "no significa nada":
// el elemento está presente por construcción, e isOptimal dice cómo salió.
//
// El elemento se identifica por el id del catálogo (vehicle_conditions_cat),
// no por su nombre: así renombrar "Tapón de gasolina" no rompe el historial.
class InspectionItem
{
public:
    InspectionItem() = default;
    InspectionItem(int elementId, bool optimal, QString observations = QString());

    int elementId() const;
    [[nodiscard]] bool setElementId(int id);

    bool isOptimal() const;
    void setOptimal(bool value);

    const QString &observations() const;
    void setObservations(const QString &value);

    // true cuando el elemento que la unidad trae salió mal. Es una CONSULTA
    // para que
    // la interfaz pueda sugerir que se escriba el detalle, no un requisito:
    // el Paso 2 no tiene campos obligatorios por decisión de negocio, y
    // convertir esto en error de validación bloquearía una captura que hoy es
    // legítima.
    bool hasFault() const;

    ValidationResult validate() const;

private:
    int m_elementId = -1;
    bool m_isOptimal = true;
    QString m_observations;
};

} // namespace domain

#endif // DOMAIN_INSPECTIONITEM_H
