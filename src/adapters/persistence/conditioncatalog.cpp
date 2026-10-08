#include "adapters/persistence/conditioncatalog.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

QList<ConditionCatalogItem> ConditionCatalog::load(QSqlDatabase &db, QString *errorMessage)
{
    QList<ConditionCatalogItem> items;

    QSqlQuery query(db);
    // init-db/13_vehicle_conditions_cat.sql inserta el catálogo agrupado por
    // categoría y en el orden de despliegue, y el id SERIAL conserva ese
    // orden, así que este ORDER BY ya entrega los ítems agrupados y con los
    // grupos en su orden. Ojo: un ítem que se agregue después a mano queda al
    // final, fuera de su grupo, y el Paso 2 le abriría un grupo repetido.
    if (!query.exec(QStringLiteral(
            "SELECT id, category, element "
            "FROM vehicle_conditions_cat ORDER BY id"))) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return items;
    }

    while (query.next()) {
        ConditionCatalogItem item;
        item.id = query.value(0).toInt();
        //item.itemKey = query.value(1).toString();
        item.category = query.value(1).toString();
        item.element = query.value(2).toString();
        //item.negativeLabel = query.value(4).toString();
        items << item;
    }

    return items;
}
