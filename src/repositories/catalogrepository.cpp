#include "../../include/db/catalogrepository.h"
#include "../../include/db/connectionpool.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

bool isFlatCatalogTable(const QString &tableName)
{
    static const QStringList tables = {
        QStringLiteral("brands_cat"),
        QStringLiteral("fuel_type_cat"),
        QStringLiteral("vehicle_maintenance_cat"),
    };
    return tables.contains(tableName);
}

QList<QPair<int, QString>> fetchCategoryPairs(int parentId)
{
    QList<QPair<int, QString>> list;
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());

    if (parentId <= 0) {
        query.prepare(QStringLiteral(
            "SELECT id, name FROM vehicle_categories_cat "
            "WHERE parent_id IS NULL ORDER BY name ASC"));
    } else {
        query.prepare(QStringLiteral(
            "SELECT id, name FROM vehicle_categories_cat "
            "WHERE parent_id = :parent_id ORDER BY name ASC"));
        query.bindValue(QStringLiteral(":parent_id"), parentId);
    }

    if (!query.exec()) {
        qWarning() << "CatalogRepository: error al leer categorías:" << query.lastError().text();
        return list;
    }

    while (query.next())
        list.append({query.value(0).toInt(), query.value(1).toString()});
    return list;
}

} // namespace

bool CatalogRepository::addCatalogItem(const QString &tableName, const QString &name, int &outId)
{
    if (!isFlatCatalogTable(tableName)) {
        qWarning() << "CatalogRepository: tabla no permitida:" << tableName;
        return false;
    }

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());
    query.prepare(QStringLiteral("INSERT INTO %1 (name) VALUES (:name) RETURNING id")
                      .arg(tableName));
    query.bindValue(QStringLiteral(":name"), name);
    if (!query.exec() || !query.next()) {
        qWarning() << "CatalogRepository: error al agregar en" << tableName << query.lastError().text();
        return false;
    }
    outId = query.value(0).toInt();
    return true;
}

bool CatalogRepository::updateCatalogItem(const QString &tableName, int id, const QString &name)
{
    if (!isFlatCatalogTable(tableName)) {
        qWarning() << "CatalogRepository: tabla no permitida:" << tableName;
        return false;
    }

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());
    query.prepare(QStringLiteral("UPDATE %1 SET name = :name WHERE id = :id").arg(tableName));
    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        qWarning() << "CatalogRepository: error al actualizar" << tableName << query.lastError().text();
        return false;
    }
    return true;
}

bool CatalogRepository::deleteCatalogItem(const QString &tableName, int id)
{
    if (!isFlatCatalogTable(tableName)) {
        qWarning() << "CatalogRepository: tabla no permitida:" << tableName;
        return false;
    }

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());
    query.prepare(QStringLiteral("DELETE FROM %1 WHERE id = :id").arg(tableName));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        qWarning() << "CatalogRepository: error al eliminar de" << tableName << query.lastError().text();
        return false;
    }
    return true;
}

QList<QPair<int, QString>> CatalogRepository::getParentCategories()
{
    return fetchCategoryPairs(0);
}

QList<QPair<int, QString>> CatalogRepository::getSubcategories(int parentId)
{
    return fetchCategoryPairs(parentId);
}

bool CatalogRepository::addExpenseCategory(const QString &name, const QString &description, int &outId)
{
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlQuery query(handle.database());
    query.prepare(QStringLiteral(
        "INSERT INTO operational_expense_cat (name, description) "
        "VALUES (:name, :description) RETURNING id"));
    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":description"),
                    description.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                          : QVariant(description));
    if (!query.exec() || !query.next()) {
        qWarning() << "CatalogRepository: error al agregar categoría de gasto:" << query.lastError().text();
        return false;
    }
    outId = query.value(0).toInt();
    return true;
}
