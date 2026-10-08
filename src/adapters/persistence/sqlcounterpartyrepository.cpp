#include "adapters/persistence/sqlcounterpartyrepository.h"

#include "domain/model/counterparty.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// Las columnas de texto opcionales van como NULL y no como cadena vacía, para
// no tener que preguntar "IS NULL OR = ''" en cada consulta posterior.
QVariant nullIfEmpty(const QString &value)
{
    if (value.isEmpty())
        return QVariant(QMetaType(QMetaType::QString));
    return value;
}

} // namespace

SqlCounterpartyRepository::SqlCounterpartyRepository(QSqlDatabase &db)
    : m_db(db)
{
}

int SqlCounterpartyRepository::findOrCreate(const domain::Counterparty &counterparty,
                                         QString &errorMessage)
{
    if (!counterparty.nationalId().isEmpty()) {
        QSqlQuery lookup(m_db);
        lookup.prepare(QStringLiteral(
            "SELECT id FROM counterparties WHERE national_id = :national_id LIMIT 1"));
        lookup.bindValue(QStringLiteral(":national_id"), counterparty.nationalId());
        if (!lookup.exec()) {
            errorMessage = lookup.lastError().text();
            return -1;
        }
        if (lookup.next())
            return lookup.value(0).toInt();
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO counterparties "
        "(full_name, national_id, street_address, suburb, locality, state, postal_code, phone, email) "
        "VALUES (:full_name, :national_id, :street_address, :suburb, :locality, :state, "
        ":postal_code, :phone, :email) "
        "RETURNING id"));
    insert.bindValue(QStringLiteral(":full_name"), counterparty.fullName());
    insert.bindValue(QStringLiteral(":national_id"), nullIfEmpty(counterparty.nationalId()));
    insert.bindValue(QStringLiteral(":street_address"), nullIfEmpty(counterparty.streetAddress()));
    insert.bindValue(QStringLiteral(":suburb"), nullIfEmpty(counterparty.suburb()));
    insert.bindValue(QStringLiteral(":locality"), nullIfEmpty(counterparty.locality()));
    insert.bindValue(QStringLiteral(":state"), nullIfEmpty(counterparty.state()));
    insert.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(counterparty.postalCode()));
    insert.bindValue(QStringLiteral(":phone"), nullIfEmpty(counterparty.phone()));
    insert.bindValue(QStringLiteral(":email"), nullIfEmpty(counterparty.email()));

    if (!insert.exec() || !insert.next()) {
        errorMessage = insert.lastError().text();
        return -1;
    }
    return insert.value(0).toInt();
}
