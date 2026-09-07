#include "../../include/db/conditioncatalog.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

QList<ConditionCatalogItem> ConditionCatalog::load(QSqlDatabase &db, QString *errorMessage)
{
    QList<ConditionCatalogItem> items;

    QSqlQuery query(db);
    // El sort_order que siembra ConditionCatalogSeeder es global y creciente
    // a lo largo de todo el catálogo, así que este ORDER BY ya entrega los
    // ítems agrupados por categoría y con los grupos en su orden. El `id`
    // final desempata: sin él, dos renglones con el mismo sort_order (por
    // ejemplo insertados a mano con el DEFAULT 0) saldrían en orden
    // indefinido y la lista "bailaría" entre arranques.
    if (!query.exec(QStringLiteral(
            "SELECT id, item_key, category, element, negative_label "
            "FROM vehicle_conditions_cat ORDER BY sort_order, id"))) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return items;
    }

    while (query.next()) {
        ConditionCatalogItem item;
        item.id = query.value(0).toInt();
        item.itemKey = query.value(1).toString();
        item.category = query.value(2).toString();
        item.element = query.value(3).toString();
        item.negativeLabel = query.value(4).toString();
        items << item;
    }

    return items;
}
