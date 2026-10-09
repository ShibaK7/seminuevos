#include "adapters/persistence/inventory/sqlreferencedatareader.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

SqlReferenceDataReader::SqlReferenceDataReader(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::CatalogOptionDto> SqlReferenceDataReader::readOptions(const QString &sql,
                                                                        QString *error)
{
    QList<application::CatalogOptionDto> options;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        if (!db.isOpen() || !query.exec(sql)) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            // Tercera columna opcional: el padre (solo en vehicle_categories_cat).
            while (query.next()) {
                application::CatalogOptionDto option;
                option.id = query.value(0).toInt();
                option.name = query.value(1).toString();
                if (query.record().count() > 2 && !query.value(2).isNull())
                    option.parentId = query.value(2).toInt();
                options << option;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    }
    return options;
}

QList<application::CatalogOptionDto> SqlReferenceDataReader::vehicleCategories(QString *error)
{
    return readOptions(QStringLiteral(
        "SELECT id, name, parent_id FROM vehicle_categories_cat ORDER BY name"), error);
}

QList<application::CatalogOptionDto> SqlReferenceDataReader::brands(QString *error)
{
    return readOptions(QStringLiteral("SELECT id, name FROM brands_cat ORDER BY name"), error);
}

QList<application::CatalogOptionDto> SqlReferenceDataReader::fuelTypes(QString *error)
{
    return readOptions(QStringLiteral("SELECT id, name FROM fuel_type_cat ORDER BY name"), error);
}

QList<application::ChecklistItemDto> SqlReferenceDataReader::conditionChecklist(QString *error)
{
    QList<application::ChecklistItemDto> items;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        // init-db/13_vehicle_conditions_cat.sql inserta el catálogo agrupado
        // por categoría y en orden de despliegue, y el id SERIAL conserva ese
        // orden: ORDER BY id entrega los grupos contiguos y en su orden.
        if (!db.isOpen()
            || !query.exec(QStringLiteral(
                "SELECT id, category, element FROM vehicle_conditions_cat ORDER BY id"))) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                items << application::ChecklistItemDto{query.value(0).toInt(),
                                                       query.value(1).toString(),
                                                       query.value(2).toString()};
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    }
    return items;
}

std::optional<double> SqlReferenceDataReader::umaDailyValue(QString *error)
{
    std::optional<double> value;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT value_param FROM global_configurations WHERE key_param = :key"));
        query.bindValue(QStringLiteral(":key"), QStringLiteral("UMA_DIARIA"));
        if (!db.isOpen() || !query.exec())
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        else if (query.next())
            value = query.value(0).toDouble();
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    }
    return value;
}
