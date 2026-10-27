#include "adapters/persistence/inventory/sqlmaintenancerepository.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// Suma el costo al total de la subtabla `table` y devuelve el total nuevo, o
// nullopt si la unidad no tiene renglón ahí. `table` es una de las dos
// constantes de add(), nunca algo que llegue de afuera.
std::optional<double> addToTotal(QSqlDatabase &db, const QString &table,
                                 const domain::MaintenanceRecord &record, bool *ok,
                                 QString *error)
{
    QSqlQuery query(db);
    // COALESCE: la columna tiene DEFAULT 0.00 pero admite NULL, y NULL + x es
    // NULL.
    query.prepare(QStringLiteral("UPDATE %1 SET maintenance_cost = "
                                 "COALESCE(maintenance_cost, 0) + CAST(:cost AS NUMERIC(12, 2)) "
                                 "WHERE vehicle_folio = :folio RETURNING maintenance_cost")
                      .arg(table));
    query.bindValue(QStringLiteral(":cost"), record.cost());
    query.bindValue(QStringLiteral(":folio"), record.vehicleFolio());
    if (!query.exec()) {
        *ok = false;
        *error = query.lastError().text();
        return std::nullopt;
    }
    *ok = true;
    if (query.next())
        return query.value(0).toDouble();
    return std::nullopt;
}

} // namespace

SqlMaintenanceRepository::SqlMaintenanceRepository(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::MaintenanceItemDto> SqlMaintenanceRepository::listForVehicle(int folio,
                                                                               QString *error)
{
    QList<application::MaintenanceItemDto> items;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        // LEFT JOIN: un mantenimiento sin tipo (la columna admite NULL) se
        // sigue viendo.
        query.prepare(QStringLiteral(
            "SELECT m.id, m.maintenance_date, c.name, m.cost, m.description, m.receipt_printed "
            "FROM vehicle_maintenance m "
            "LEFT JOIN vehicle_maintenance_cat c ON c.id = m.maintenance_type_id "
            "WHERE m.vehicle_folio = :folio "
            "ORDER BY m.maintenance_date DESC, m.id DESC"));
        query.bindValue(QStringLiteral(":folio"), folio);
        if (!db.isOpen() || !query.exec()) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                application::MaintenanceItemDto item;
                item.id = query.value(0).toInt();
                item.date = query.value(1).toDate();
                item.typeName = query.value(2).toString();
                item.cost = query.value(3).toDouble();
                item.description = query.value(4).toString();
                item.receiptPrinted = query.value(5).toBool();
                items << item;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        items.clear();
    }
    return items;
}

application::MaintenanceRepository::AddOutcome SqlMaintenanceRepository::add(
    const domain::MaintenanceRecord &record)
{
    AddOutcome result;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            result.errorMessage = QStringLiteral("No se pudo conectar con la base de datos: %1")
                                      .arg(db.lastError().text());
        } else if (!db.transaction()) {
            result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1")
                                      .arg(db.lastError().text());
        } else {
            QString error;
            bool ok = false;
            {
                QSqlQuery insert(db);
                insert.prepare(QStringLiteral(
                    "INSERT INTO vehicle_maintenance "
                    "(vehicle_folio, maintenance_date, maintenance_type_id, cost, description) "
                    "VALUES (:folio, :date, :type, CAST(:cost AS NUMERIC(12, 2)), :description) "
                    "RETURNING id"));
                insert.bindValue(QStringLiteral(":folio"), record.vehicleFolio());
                insert.bindValue(QStringLiteral(":date"), record.date());
                insert.bindValue(QStringLiteral(":type"), record.typeId());
                insert.bindValue(QStringLiteral(":cost"), record.cost());
                // Sin descripción se guarda NULL, no una cadena vacía.
                insert.bindValue(QStringLiteral(":description"),
                                 record.description().isEmpty()
                                     ? QVariant(QMetaType::fromType<QString>())
                                     : QVariant(record.description()));
                ok = insert.exec() && insert.next();
                if (ok)
                    result.id = insert.value(0).toInt();
                else
                    error = insert.lastError().text();
            }

            // La unidad tiene renglón en una sola de las dos subtablas: la
            // primera que actualiza algo es la suya.
            if (ok)
                result.maintenanceCostTotal = addToTotal(
                    db, QStringLiteral("vehicle_acquisitions"), record, &ok, &error);
            if (ok && !result.maintenanceCostTotal)
                result.maintenanceCostTotal = addToTotal(
                    db, QStringLiteral("vehicle_consignments"), record, &ok, &error);

            if (!ok) {
                db.rollback();
                result.errorMessage =
                    QStringLiteral("No se pudo guardar el mantenimiento: %1").arg(error);
            } else if (!db.commit()) {
                result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1")
                                          .arg(db.lastError().text());
                db.rollback();
            } else {
                result.ok = true;
            }
        }
    }
    if (!result.ok) {
        // Con la conexión ya soltada, una que falló se descarta para que el
        // reintento abra otra.
        m_pool.discardThreadConnection();
        result.id = -1;
        result.maintenanceCostTotal.reset();
    }
    return result;
}

bool SqlMaintenanceRepository::updateOne(const QString &sql,
                                         const QList<std::pair<QString, QVariant>> &values,
                                         QString *error)
{
    QString failure;
    bool databaseFailed = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(sql);
        for (const auto &[name, value] : values)
            query.bindValue(name, value);
        if (!db.isOpen() || !query.exec()) {
            databaseFailed = true;
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else if (query.numRowsAffected() != 1) {
            failure = QStringLiteral("El mantenimiento ya no existe.");
        }
    }
    // Solo una falla de la base descarta la conexión: un renglón que ya no
    // existe no dice nada de ella.
    if (databaseFailed)
        m_pool.discardThreadConnection();
    if (!failure.isEmpty() && error)
        *error = failure;
    return failure.isEmpty();
}

bool SqlMaintenanceRepository::updateDescription(int id, const QString &description,
                                                 QString *error)
{
    return updateOne(
        QStringLiteral("UPDATE vehicle_maintenance SET description = :description WHERE id = :id"),
        {{QStringLiteral(":description"), description}, {QStringLiteral(":id"), id}}, error);
}

bool SqlMaintenanceRepository::markReceiptPrinted(int id, QString *error)
{
    return updateOne(
        QStringLiteral("UPDATE vehicle_maintenance SET receipt_printed = TRUE WHERE id = :id"),
        {{QStringLiteral(":id"), id}}, error);
}
