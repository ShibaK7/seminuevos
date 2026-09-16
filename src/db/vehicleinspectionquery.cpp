#include "../../include/db/vehicleinspectionquery.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

enum Column {
    ColId = 0,
    ColItemKey,
    ColCategory,
    ColElement,
    ColNegativeLabel,
    ColIsPresent,
    ColIsOptimal,
    ColObservations,
};

// El checklist se arma desde el CATÁLOGO hacia la inspección, no al revés:
// vehicle_inspection solo tiene los elementos que la unidad trae, así que
// recorrerla dejaría fuera justamente los que interesan.
//
// OJO CON EL FILTRO POR VEHÍCULO: `i.vehicle_folio = :folio` va en el ON y no
// en el WHERE. En el WHERE se evalúa DESPUÉS del join, cuando las filas sin
// pareja ya traen i.vehicle_folio en NULL; NULL = :folio no es verdadero, así
// que se descartarían y el LEFT JOIN se comportaría como un INNER JOIN. Es
// decir: el filtro mal puesto elimina exactamente las filas que este módulo
// existe para encontrar.
//
// No hay riesgo de duplicar renglones del catálogo: el UNIQUE
// (vehicle_folio, element_id) garantiza a lo sumo una pareja por elemento.
//
// El ORDER BY es el mismo de ConditionCatalog::load, para que la pantalla de
// detalle muestre los ítems en el orden en que se capturaron.
constexpr auto kSelect = R"SQL(
SELECT c.id,
       c.item_key,
       c.category,
       c.element,
       c.negative_label,
       (i.id IS NOT NULL) AS is_present,
       i.is_optimal,
       i.observations
  FROM vehicle_conditions_cat c
  LEFT JOIN vehicle_inspection i
         ON i.element_id = c.id
        AND i.vehicle_folio = :folio
 ORDER BY c.sort_order, c.id
)SQL";

// Misma consulta, quedándose solo con las filas sin pareja. El filtro va en el
// WHERE a propósito -- aquí sí -- porque probar IS NULL sobre el lado derecho
// es precisamente cómo se pide "lo que no empareja" (antijoin).
constexpr auto kSelectMissing = R"SQL(
SELECT c.id,
       c.item_key,
       c.category,
       c.element,
       c.negative_label
  FROM vehicle_conditions_cat c
  LEFT JOIN vehicle_inspection i
         ON i.element_id = c.id
        AND i.vehicle_folio = :folio
 WHERE i.id IS NULL
 ORDER BY c.sort_order, c.id
)SQL";

ConditionCatalogItem readCatalogItem(const QSqlQuery &query)
{
    ConditionCatalogItem item;
    item.id = query.value(ColId).toInt();
    item.itemKey = query.value(ColItemKey).toString();
    item.category = query.value(ColCategory).toString();
    item.element = query.value(ColElement).toString();
    item.negativeLabel = query.value(ColNegativeLabel).toString();
    return item;
}

bool runForFolio(QSqlQuery &query, const char *sql, int folio, QString *errorMessage)
{
    if (!query.prepare(QString::fromLatin1(sql))) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":folio"), folio);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

} // namespace

QList<VehicleInspectionRow> VehicleInspectionQuery::load(QSqlDatabase &db, int folio,
                                                         QString *errorMessage)
{
    QList<VehicleInspectionRow> rows;

    QSqlQuery query(db);
    if (!runForFolio(query, kSelect, folio, errorMessage))
        return rows;

    while (query.next()) {
        VehicleInspectionRow row;
        row.element = readCatalogItem(query);
        row.isPresent = query.value(ColIsPresent).toBool();
        // Se leen solo cuando hay renglón: con el LEFT JOIN sin pareja vienen
        // en NULL, y toBool() sobre un QVariant nulo devuelve false, que se
        // interpretaría como "lo trae y está en mal estado".
        if (row.isPresent) {
            row.isOptimal = query.value(ColIsOptimal).toBool();
            row.observations = query.value(ColObservations).toString();
        }
        rows << row;
    }

    return rows;
}

QList<ConditionCatalogItem> VehicleInspectionQuery::loadMissing(QSqlDatabase &db, int folio,
                                                                QString *errorMessage)
{
    QList<ConditionCatalogItem> items;

    QSqlQuery query(db);
    if (!runForFolio(query, kSelectMissing, folio, errorMessage))
        return items;

    while (query.next())
        items << readCatalogItem(query);

    return items;
}
