#ifndef DOMAIN_INSPECTIONITEM_H
#define DOMAIN_INSPECTIONITEM_H

#include "domain/validationresult.h"

#include <QString>

namespace domain {

// El resultado de revisar UN elemento del checklist en un vehículo. Mapea un
// renglón de vehicle_inspection.
//
// isChecked e isOptimal son dos ejes distintos, no dos formas de decir lo
// mismo:
//   isChecked == false -> la unidad no trae ese accesorio (no hay antena que
//                         revisar). isOptimal no significa nada.
//   isChecked == true  -> se revisó, e isOptimal dice cómo salió.
// Colapsarlos en un solo booleano haría indistinguible "no lo trae" de "lo
// trae y está bien".
//
// El elemento se identifica por el id del catálogo (vehicle_conditions_cat),
// no por su nombre: así renombrar "Tapón de gasolina" no rompe el historial.
class InspectionItem
{
public:
    InspectionItem() = default;
    InspectionItem(int elementId, bool checked, bool optimal, QString observations = QString());

    int elementId() const;
    [[nodiscard]] bool setElementId(int id);

    bool isChecked() const;
    void setChecked(bool value);

    bool isOptimal() const;
    void setOptimal(bool value);

    const QString &observations() const;
    void setObservations(const QString &value);

    // true cuando el elemento se revisó y salió mal. Es una CONSULTA para que
    // la interfaz pueda sugerir que se escriba el detalle, no un requisito:
    // el Paso 2 no tiene campos obligatorios por decisión de negocio, y
    // convertir esto en error de validación bloquearía una captura que hoy es
    // legítima.
    bool hasFault() const;

    ValidationResult validate() const;

private:
    int m_elementId = -1;
    bool m_isChecked = true;
    bool m_isOptimal = true;
    QString m_observations;
};

} // namespace domain

#endif // DOMAIN_INSPECTIONITEM_H
