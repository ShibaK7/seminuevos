#include "domain/inventory/model/inspection.h"

namespace domain {

bool Inspection::addItem(const InspectionItem &item)
{
    if (item.elementId() <= 0)
        return false;

    for (const InspectionItem &existing : m_items) {
        if (existing.elementId() == item.elementId())
            return false;
    }

    m_items.append(item);
    return true;
}

void Inspection::setItem(const InspectionItem &item)
{
    if (item.elementId() <= 0)
        return;

    for (InspectionItem &existing : m_items) {
        if (existing.elementId() == item.elementId()) {
            existing = item;
            return;
        }
    }
    m_items.append(item);
}

bool Inspection::removeItem(int elementId)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).elementId() == elementId) {
            m_items.removeAt(i);
            return true;
        }
    }
    return false;
}

void Inspection::clear()
{
    m_items.clear();
}

const QList<InspectionItem> &Inspection::items() const
{
    return m_items;
}

bool Inspection::isEmpty() const
{
    return m_items.isEmpty();
}

int Inspection::itemCount() const
{
    return static_cast<int>(m_items.size());
}

int Inspection::faultCount() const
{
    int count = 0;
    for (const InspectionItem &item : m_items) {
        if (item.hasFault())
            ++count;
    }
    return count;
}

bool Inspection::isFlawless() const
{
    // Una inspección vacía no es una unidad impecable: es una unidad que no
    // se revisó.
    return !m_items.isEmpty() && faultCount() == 0;
}

double Inspection::optimalRatio() const
{
    const int present = itemCount();
    if (present == 0)
        return 0.0;
    return static_cast<double>(present - faultCount()) / static_cast<double>(present);
}

QList<int> Inspection::faultyElementIds() const
{
    QList<int> ids;
    for (const InspectionItem &item : m_items) {
        if (item.hasFault())
            ids << item.elementId();
    }
    return ids;
}

ValidationResult Inspection::validate() const
{
    ValidationResult result;
    for (int i = 0; i < m_items.size(); ++i)
        result.merge(m_items.at(i).validate(), QStringLiteral("items[%1]").arg(i));
    return result;
}

} // namespace domain
