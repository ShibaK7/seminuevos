#include "adapters/persistence/inventory/sqlcounterpartyrepository.h"

#include "domain/inventory/model/counterparty.h"

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

// Dos capturas del mismo nombre no siempre coinciden letra por letra: los
// espacios de más y las mayúsculas no hacen a otra persona.
bool sameName(const QString &a, const QString &b)
{
    return QString::compare(a.simplified(), b.simplified(), Qt::CaseInsensitive) == 0;
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
        // ORDER BY id: si por datos viejos la identificación se repite, siempre
        // se elige la misma persona (la más antigua).
        QSqlQuery lookup(m_db);
        lookup.prepare(QStringLiteral(
            "SELECT id FROM counterparties WHERE national_id = :national_id ORDER BY id LIMIT 1"));
        lookup.bindValue(QStringLiteral(":national_id"), counterparty.nationalId());
        if (!lookup.exec()) {
            errorMessage = lookup.lastError().text();
            return -1;
        }
        if (lookup.next())
            return lookup.value(0).toInt();
    }
    return insert(counterparty, errorMessage);
}

SqlCounterpartyRepository::Relink SqlCounterpartyRepository::keepOrRelink(
    int currentId, const domain::Counterparty &edited, const QString &expectedVersion)
{
    Relink result;

    // FOR UPDATE: el renglón queda bloqueado hasta el fin de la transacción,
    // así nadie lo cambia entre revisar su versión y corregirlo.
    QSqlQuery current(m_db);
    current.prepare(QStringLiteral("SELECT national_id, CAST(xmin AS text) FROM counterparties "
                                   "WHERE id = :id FOR UPDATE"));
    current.bindValue(QStringLiteral(":id"), currentId);
    if (!current.exec()) {
        result.errorMessage = current.lastError().text();
        return result;
    }
    if (!current.next()) {
        result.errorMessage = QStringLiteral("La contraparte %1 ya no existe.").arg(currentId);
        return result;
    }

    // La misma identificación es la misma persona: se corrige donde está, si
    // nadie la corrigió desde que se leyó.
    const QString currentNationalId = current.value(0).toString().trimmed().toUpper();
    if (currentNationalId == edited.nationalId()) {
        if (current.value(1).toString() != expectedVersion) {
            result.conflict = true;
            return result;
        }
        QSqlQuery update(m_db);
        update.prepare(QStringLiteral(
            "UPDATE counterparties "
            "   SET full_name = :full_name, street_address = :street_address, suburb = :suburb, "
            "       locality = :locality, state = :state, postal_code = :postal_code, "
            "       updated_at = CURRENT_TIMESTAMP "
            " WHERE id = :id"));
        update.bindValue(QStringLiteral(":full_name"), edited.fullName());
        update.bindValue(QStringLiteral(":street_address"), nullIfEmpty(edited.streetAddress()));
        update.bindValue(QStringLiteral(":suburb"), nullIfEmpty(edited.suburb()));
        update.bindValue(QStringLiteral(":locality"), nullIfEmpty(edited.locality()));
        update.bindValue(QStringLiteral(":state"), nullIfEmpty(edited.state()));
        update.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(edited.postalCode()));
        update.bindValue(QStringLiteral(":id"), currentId);
        if (!update.exec()) {
            result.errorMessage = update.lastError().text();
            return result;
        }
        result.id = currentId;
        return result;
    }

    // Otra identificación: la persona que ya la tiene, si es la misma que se
    // capturó, o una nueva.
    if (!edited.nationalId().isEmpty()) {
        QSqlQuery owner(m_db);
        owner.prepare(QStringLiteral("SELECT id, full_name FROM counterparties "
                                     "WHERE national_id = :national_id ORDER BY id LIMIT 1"));
        owner.bindValue(QStringLiteral(":national_id"), edited.nationalId());
        if (!owner.exec()) {
            result.errorMessage = owner.lastError().text();
            return result;
        }
        if (owner.next()) {
            const QString ownerName = owner.value(1).toString();
            if (!sameName(ownerName, edited.fullName())) {
                result.takenBy = ownerName;
                return result;
            }
            result.id = owner.value(0).toInt();
            return result;
        }
    }
    result.id = insert(edited, result.errorMessage);
    return result;
}

int SqlCounterpartyRepository::insert(const domain::Counterparty &counterparty,
                                      QString &errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO counterparties "
        "(full_name, national_id, street_address, suburb, locality, state, postal_code, phone, email) "
        "VALUES (:full_name, :national_id, :street_address, :suburb, :locality, :state, "
        ":postal_code, :phone, :email) "
        "RETURNING id"));
    query.bindValue(QStringLiteral(":full_name"), counterparty.fullName());
    query.bindValue(QStringLiteral(":national_id"), nullIfEmpty(counterparty.nationalId()));
    query.bindValue(QStringLiteral(":street_address"), nullIfEmpty(counterparty.streetAddress()));
    query.bindValue(QStringLiteral(":suburb"), nullIfEmpty(counterparty.suburb()));
    query.bindValue(QStringLiteral(":locality"), nullIfEmpty(counterparty.locality()));
    query.bindValue(QStringLiteral(":state"), nullIfEmpty(counterparty.state()));
    query.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(counterparty.postalCode()));
    query.bindValue(QStringLiteral(":phone"), nullIfEmpty(counterparty.phone()));
    query.bindValue(QStringLiteral(":email"), nullIfEmpty(counterparty.email()));

    if (!query.exec() || !query.next()) {
        errorMessage = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}
