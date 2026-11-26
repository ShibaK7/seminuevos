#include "adapters/persistence/inventory/sqlreferencedatareader.h"

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/settings/settingskeys.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include <cmath>

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

QList<application::CatalogOptionDto> SqlReferenceDataReader::maintenanceTypes(QString *error)
{
    return readOptions(
        QStringLiteral("SELECT id, name FROM vehicle_maintenance_cat ORDER BY name"), error);
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

domain::CashLimitTerms SqlReferenceDataReader::cashLimitTerms(QString *error)
{
    // Sin renglón de UMA queda en 0 ("no configurada"). Sin tope, o con uno
    // que no sea un entero entre 1 y el legal, queda el legal: la regla
    // tampoco acepta uno mayor.
    domain::CashLimitTerms terms;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT key_param, value_param FROM global_configurations "
                                     "WHERE key_param IN (:uma, :multiple)"));
        query.bindValue(QStringLiteral(":uma"), settingskeys::umaDailyValue());
        query.bindValue(QStringLiteral(":multiple"), settingskeys::cashLimitUmaMultiple());
        if (!db.isOpen() || !query.exec()) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                const double value = query.value(1).toDouble();
                if (query.value(0).toString() == settingskeys::umaDailyValue()) {
                    terms.umaDailyValue = value;
                } else if (value >= 1.0 && value == std::floor(value)
                           && value <= domain::CashLimitTerms::kLegalUmaMultiple) {
                    terms.umaMultiple = static_cast<int>(value);
                }
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    }
    return terms;
}
