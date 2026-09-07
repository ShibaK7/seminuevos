#include "domain/inspectionitem.h"

#include <utility>

namespace domain {

InspectionItem::InspectionItem(int elementId, bool checked, bool optimal, QString observations)
    : m_elementId(elementId)
    , m_isChecked(checked)
    , m_isOptimal(optimal)
    , m_observations(std::move(observations))
{
}

int InspectionItem::elementId() const
{
    return m_elementId;
}

bool InspectionItem::setElementId(int id)
{
    // Los ids de vehicle_conditions_cat vienen de un SERIAL, así que
    // arrancan en 1. Un 0 o un negativo solo pueden venir de un valor por
    // omisión que nadie llenó, y llegarían a la base como violación de llave
    // foránea con un mensaje mucho menos claro que este rechazo.
    if (id <= 0)
        return false;
    m_elementId = id;
    return true;
}

bool InspectionItem::isChecked() const
{
    return m_isChecked;
}

void InspectionItem::setChecked(bool value)
{
    m_isChecked = value;
}

bool InspectionItem::isOptimal() const
{
    return m_isOptimal;
}

void InspectionItem::setOptimal(bool value)
{
    m_isOptimal = value;
}

const QString &InspectionItem::observations() const
{
    return m_observations;
}

void InspectionItem::setObservations(const QString &value)
{
    m_observations = value.trimmed();
}

bool InspectionItem::hasFault() const
{
    return m_isChecked && !m_isOptimal;
}

ValidationResult InspectionItem::validate() const
{
    ValidationResult result;
    if (m_elementId <= 0) {
        result.addError(QStringLiteral("elementId"),
                        QStringLiteral("El elemento revisado no tiene un identificador de "
                                       "catálogo válido."));
    }
    return result;
}

} // namespace domain
