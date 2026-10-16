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

// Hasta que termine la transacción, nadie más busca ni da de alta a una
// persona con esta identificación. Sin esto, dos sesiones que no la encuentran
// al mismo tiempo crearían dos personas con ella: la tabla no la tiene como
// UNIQUE. Es un candado consultivo y no uno de renglón porque lo que se cuida
// es un renglón que todavía no existe. La primera llave (la tabla) lo aparta
// de cualquier otro candado consultivo; la segunda es la identificación.
bool lockNationalId(QSqlDatabase &db, const QString &nationalId, QString &errorMessage)
{
    QSqlQuery lock(db);
    lock.prepare(QStringLiteral("SELECT pg_advisory_xact_lock("
                                "CAST('counterparties'::regclass AS integer), hashtext(:national_id))"));
    lock.bindValue(QStringLiteral(":national_id"), nationalId);
    if (!lock.exec()) {
        errorMessage = lock.lastError().text();
        return false;
    }
    return true;
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
        if (!lockNationalId(m_db, counterparty.nationalId(), errorMessage))
            return -1;
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

    QSqlQuery current(m_db);
    current.prepare(QStringLiteral("SELECT national_id FROM counterparties WHERE id = :id"));
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
        // La versión va en el WHERE: revisarla y corregir es un solo paso. Si
        // otra sesión está corrigiendo a la misma persona, este UPDATE la
        // espera, y cuando ella confirma PostgreSQL vuelve a evaluar el WHERE
        // con el renglón nuevo. La versión ya no coincide, no cambia nada y es
        // un conflicto. Con un SELECT aparte para revisarla, las dos sesiones
        // podrían leer la misma versión y la segunda pisaría a la primera.
        //
        // Como el UPDATE no cambia la llave, su candado es FOR NO KEY UPDATE,
        // que no choca con el FOR KEY SHARE que pide la llave foránea cuando
        // otra operación se liga a esta persona. Con FOR UPDATE, dos unidades
        // religadas al mismo tiempo en sentidos opuestos (la 10 a la persona
        // de la 11 y viceversa) se bloqueaban entre sí.
        QSqlQuery update(m_db);
        update.prepare(QStringLiteral(
            "UPDATE counterparties "
            "   SET full_name = :full_name, street_address = :street_address, suburb = :suburb, "
            "       locality = :locality, state = :state, postal_code = :postal_code, "
            "       updated_at = CURRENT_TIMESTAMP "
            " WHERE id = :id AND CAST(xmin AS text) = :version"));
        update.bindValue(QStringLiteral(":full_name"), edited.fullName());
        update.bindValue(QStringLiteral(":street_address"), nullIfEmpty(edited.streetAddress()));
        update.bindValue(QStringLiteral(":suburb"), nullIfEmpty(edited.suburb()));
        update.bindValue(QStringLiteral(":locality"), nullIfEmpty(edited.locality()));
        update.bindValue(QStringLiteral(":state"), nullIfEmpty(edited.state()));
        update.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(edited.postalCode()));
        update.bindValue(QStringLiteral(":id"), currentId);
        update.bindValue(QStringLiteral(":version"), expectedVersion);
        if (!update.exec()) {
            result.errorMessage = update.lastError().text();
            return result;
        }
        // Ya se vio que existe (y la llave foránea impide borrarla): si no
        // cambió nada, es que alguien la corrigió desde que se leyó.
        if (update.numRowsAffected() != 1) {
            result.conflict = true;
            return result;
        }
        result.id = currentId;
        return result;
    }

    // Otra identificación: la persona que ya la tiene, si es la misma que se
    // capturó, o una nueva.
    if (!edited.nationalId().isEmpty()) {
        if (!lockNationalId(m_db, edited.nationalId(), result.errorMessage))
            return result;
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
