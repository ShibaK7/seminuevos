#ifndef CATALOGREPOSITORY_H
#define CATALOGREPOSITORY_H

#include <QList>
#include <QPair>
#include <QString>

// Escritura de catálogos planos (Rule 2) y lecturas jerárquicas de
// vehicle_categories_cat. Los combos planos van por QSqlQuery (Rule 1).
//
// add/update/delete solo aceptan: brands_cat, fuel_type_cat,
// vehicle_maintenance_cat.
class CatalogRepository
{
public:
    CatalogRepository() = delete;

    static bool addCatalogItem(const QString &tableName, const QString &name, int &outId);
    static bool updateCatalogItem(const QString &tableName, int id, const QString &name);
    static bool deleteCatalogItem(const QString &tableName, int id);

    static QList<QPair<int, QString>> getParentCategories();
    static QList<QPair<int, QString>> getSubcategories(int parentId);

    static bool addExpenseCategory(const QString &name, const QString &description, int &outId);
};

#endif // CATALOGREPOSITORY_H
