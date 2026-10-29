#include "adapters/persistence/common/sqlcounterpartydirectory.h"

#include "adapters/persistence/connectionpool.h"
#include "domain/inventory/model/counterparty.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

application::CounterpartyDto readCounterparty(const QSqlQuery &query)
{
    application::CounterpartyDto result;
    result.id = query.value(0).toInt();
    result.fullName = query.value(1).toString();
    result.nationalId = query.value(2).toString();
    result.streetAddress = query.value(3).toString();
    result.suburb = query.value(4).toString();
    result.locality = query.value(5).toString();
    result.state = query.value(6).toString();
    result.postalCode = query.value(7).toString();
    result.phone = query.value(8).toString();
    result.email = query.value(9).toString();
    return result;
}

QVariant nullIfEmpty(const QString &value)
{
    if (value.isEmpty())
        return QVariant(QMetaType(QMetaType::QString));
    return value;
}

bool applyToDomain(const application::CounterpartyDto &source, domain::Counterparty &target,
                   QString &field)
{
    target.assignId(source.id);
    if (!target.setFullName(source.fullName))
        field = QStringLiteral("fullName");
    else if (!target.setNationalId(source.nationalId))
        field = QStringLiteral("nationalId");
    else {
        target.setStreetAddress(source.streetAddress);
        if (!target.setSuburb(source.suburb))
            field = QStringLiteral("suburb");
        else if (!target.setLocality(source.locality))
            field = QStringLiteral("locality");
        else if (!target.setState(source.state))
            field = QStringLiteral("state");
        else if (!target.setPostalCode(source.postalCode))
            field = QStringLiteral("postalCode");
        else if (!target.setPhone(source.phone))
            field = QStringLiteral("phone");
        else if (!target.setEmail(source.email))
            field = QStringLiteral("email");
    }
    return field.isEmpty();
}

bool nationalIdAlreadyExists(QSqlDatabase &db, const application::CounterpartyDto &counterparty,
                            QString *error)
{
    const QString trimmed = counterparty.nationalId.trimmed();
    if (trimmed.isEmpty())
        return false;

    QSqlQuery sql(db);
    sql.prepare(QStringLiteral(
        "SELECT id FROM counterparties "
        "WHERE UPPER(BTRIM(COALESCE(national_id, ''))) = :national_id "
        "AND id <> :id LIMIT 1"));
    sql.bindValue(QStringLiteral(":national_id"), trimmed.toUpper());
    sql.bindValue(QStringLiteral(":id"), counterparty.id > 0 ? counterparty.id : -1);
    if (!sql.exec()) {
        if (error)
            *error = sql.lastError().text();
        return true;
    }
    if (sql.next())
        return true;
    return false;
}

} // namespace

SqlCounterpartyDirectory::SqlCounterpartyDirectory(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::CounterpartyDto> SqlCounterpartyDirectory::search(const QString &query,
                                                                      QString *error)
{
    QList<application::CounterpartyDto> results;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery sql(db);
            sql.prepare(QStringLiteral(
                "SELECT id, full_name, national_id, street_address, suburb, locality, state, "
                "postal_code, phone, email FROM counterparties "
                "WHERE UPPER(BTRIM(COALESCE(national_id, ''))) = :national_id "
                "   OR full_name ILIKE :name "
                "ORDER BY CASE WHEN UPPER(BTRIM(COALESCE(national_id, ''))) = :sort_id "
                "THEN 0 ELSE 1 END, full_name, id LIMIT 20"));
            const QString normalized = query.trimmed().toUpper();
            sql.bindValue(QStringLiteral(":national_id"), normalized);
            sql.bindValue(QStringLiteral(":sort_id"), normalized);
            sql.bindValue(QStringLiteral(":name"), QStringLiteral("%") + query.trimmed()
                                                                       + QStringLiteral("%"));
            if (!sql.exec()) {
                failure = sql.lastError().text();
            } else {
                while (sql.next())
                    results << readCounterparty(sql);
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    } else if (error) {
        error->clear();
    }
    return results;
}

application::CounterpartyUpdateResultDto SqlCounterpartyDirectory::update(
    const application::CounterpartyDto &counterparty)
{
    application::CounterpartyUpdateResultDto result;
    domain::Counterparty validated;
    if (!applyToDomain(counterparty, validated, result.field)) {
        result.message = QStringLiteral("El campo no cumple con el formato permitido.");
        return result;
    }
    const domain::ValidationResult validation = validated.validate();
    if (!validation.isValid()) {
        result.field = QStringLiteral("counterparty");
        result.message = QStringLiteral("El nombre y la identificación son obligatorios.");
        return result;
    }

    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else if (nationalIdAlreadyExists(db, counterparty, &failure)) {
            result.field = QStringLiteral("counterparty.nationalId");
            result.message = QStringLiteral("Ya existe una persona registrada con ese número de identificación.");
            return result;
        } else if (!db.transaction()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery sql(db);
            sql.prepare(QStringLiteral(
                "UPDATE counterparties SET full_name = :full_name, national_id = :national_id, "
                "street_address = :street_address, suburb = :suburb, locality = :locality, "
                "state = :state, postal_code = :postal_code, phone = :phone, email = :email, "
                "updated_at = CURRENT_TIMESTAMP WHERE id = :id"));
            sql.bindValue(QStringLiteral(":full_name"), validated.fullName());
            sql.bindValue(QStringLiteral(":national_id"), validated.nationalId());
            sql.bindValue(QStringLiteral(":street_address"), nullIfEmpty(validated.streetAddress()));
            sql.bindValue(QStringLiteral(":suburb"), nullIfEmpty(validated.suburb()));
            sql.bindValue(QStringLiteral(":locality"), nullIfEmpty(validated.locality()));
            sql.bindValue(QStringLiteral(":state"), nullIfEmpty(validated.state()));
            sql.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(validated.postalCode()));
            sql.bindValue(QStringLiteral(":phone"), nullIfEmpty(validated.phone()));
            sql.bindValue(QStringLiteral(":email"), nullIfEmpty(validated.email()));
            sql.bindValue(QStringLiteral(":id"), validated.id());
            if (!sql.exec()) {
                failure = sql.lastError().text();
                db.rollback();
            } else if (sql.numRowsAffected() != 1) {
                failure = QStringLiteral("No se encontró la persona seleccionada.");
                db.rollback();
            } else if (!db.commit()) {
                failure = db.lastError().text();
                db.rollback();
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        result.message = failure;
        return result;
    }
    result.ok = true;
    return result;
}