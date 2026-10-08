#ifndef ADAPTERS_PERSISTENCE_CONDITIONCATALOG_H
#define ADAPTERS_PERSISTENCE_CONDITIONCATALOG_H

#include <QList>
#include <QString>

class QSqlDatabase;

// Un renglón de vehicle_conditions_cat tal como lo consume la UI del Paso 2.
// `id` es el identificador real del ítem y es lo que se guarda en
// vehicle_inspection.element_id.
struct ConditionCatalogItem
{
    int id = -1;
    QString itemKey;
    QString category;        // grupo del checklist: "Exteriores", "Mecánica"...
    QString element;         // etiqueta visible del checkbox
    QString negativeLabel;   // etiqueta del radio negativo, varía por ítem
};

namespace ConditionCatalog
{
// Devuelve el catálogo ordenado, con las categorías contiguas y en el orden
// de despliegue. Lista vacía = catálogo sin sembrar o error de consulta; el
// llamador debe mostrarlo al usuario, no seguir en silencio (sin catálogo el
// Paso 2 queda en blanco). Si se pasa errorMessage, recibe el detalle.
QList<ConditionCatalogItem> load(QSqlDatabase &db, QString *errorMessage = nullptr);
} // namespace ConditionCatalog

#endif // ADAPTERS_PERSISTENCE_CONDITIONCATALOG_H
