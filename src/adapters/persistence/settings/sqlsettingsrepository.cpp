#include "adapters/persistence/settings/sqlsettingsrepository.h"

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/settings/settingskeys.h"
#include "domain/common/value_objects/enums.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

SqlSettingsRepository::SqlSettingsRepository(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::SystemParameterDto> SqlSettingsRepository::getSystemParameters(QString *error)
{
    QList<application::SystemParameterDto> parameters;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QStringList placeholders;
        const QList<domain::SystemParameter> all = domain::allSystemParameters();
        for (qsizetype i = 0; i < all.size(); ++i)
            placeholders << QStringLiteral(":k%1").arg(i);
        QSqlQuery query(db);
        // En centésimos y como entero: comparar si cambió es exacto.
        query.prepare(QStringLiteral("SELECT key_param, CAST(ROUND(value_param * 100) AS BIGINT), "
                                     "updated_at FROM global_configurations "
                                     "WHERE key_param IN (%1)")
                          .arg(placeholders.join(QStringLiteral(", "))));
        for (qsizetype i = 0; i < all.size(); ++i)
            query.bindValue(placeholders.at(i), settingskeys::keyOf(all.at(i)));
        if (!db.isOpen() || !query.exec()) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                const std::optional<domain::SystemParameter> parameter =
                    settingskeys::parameterOf(query.value(0).toString());
                if (!parameter)
                    continue;
                application::SystemParameterDto dto;
                dto.parameter = *parameter;
                dto.hundredths = query.value(1).toLongLong();
                dto.updatedAt = query.value(2).toDateTime();
                parameters << dto;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        parameters.clear();
    } else if (error) {
        error->clear();
    }
    return parameters;
}

application::SettingsRepository::UpdateOutcome SqlSettingsRepository::updateSystemParameters(
    const QList<Change> &changes, int actorId)
{
    using Status = UpdateOutcome::Status;
    UpdateOutcome result;
    bool databaseFailed = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            databaseFailed = true;
            result.errorMessage = QStringLiteral("No se pudo conectar con la base de datos: %1")
                                      .arg(db.lastError().text());
        } else if (!db.transaction()) {
            databaseFailed = true;
            result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1")
                                      .arg(db.lastError().text());
        } else {
            result.status = Status::Updated;
            // Quien guarda sigue siendo Administrador activo. FOR SHARE: si
            // otro administrador le está quitando el rol, espera a que
            // confirme y decide con lo vigente.
            QSqlQuery actor(db);
            actor.prepare(QStringLiteral("SELECT 1 FROM users WHERE id = :id AND active "
                                         "AND role = :admin FOR SHARE"));
            actor.bindValue(QStringLiteral(":id"), actorId);
            actor.bindValue(QStringLiteral(":admin"), domain::toDbString(domain::UserRole::Admin));
            if (!actor.exec()) {
                databaseFailed = true;
                result.status = Status::Failed;
                result.errorMessage = actor.lastError().text();
            } else if (!actor.next()) {
                result.status = Status::ActorNotAllowed;
            }

            for (const Change &change : changes) {
                if (result.status != Status::Updated)
                    break;
                QSqlQuery query(db);
                if (change.expected) {
                    // El valor esperado va en el WHERE: si otro administrador
                    // lo está cambiando, este UPDATE lo espera y, cuando
                    // confirma, PostgreSQL vuelve a evaluar el WHERE con el
                    // renglón nuevo. Ya no coincide y no se pisa.
                    query.prepare(QStringLiteral(
                        "UPDATE global_configurations "
                        "   SET value_param = CAST(:value AS NUMERIC) / 100, "
                        "       updated_at = CURRENT_TIMESTAMP "
                        " WHERE key_param = :key AND value_param * 100 = :expected"));
                    query.bindValue(QStringLiteral(":expected"), *change.expected);
                } else {
                    // Nunca se había capturado: se crea, salvo que otro
                    // administrador lo haya creado mientras tanto.
                    query.prepare(QStringLiteral(
                        "INSERT INTO global_configurations (key_param, value_param, updated_at) "
                        "VALUES (:key, CAST(:value AS NUMERIC) / 100, CURRENT_TIMESTAMP) "
                        "ON CONFLICT (key_param) DO NOTHING"));
                }
                query.bindValue(QStringLiteral(":key"), settingskeys::keyOf(change.parameter));
                query.bindValue(QStringLiteral(":value"), change.value);
                if (!query.exec()) {
                    databaseFailed = true;
                    result.status = Status::Failed;
                    result.errorMessage = query.lastError().text();
                    break;
                }
                if (query.numRowsAffected() != 1) {
                    result.status = Status::Conflict;
                    break;
                }
            }

            if (result.status != Status::Updated) {
                db.rollback();
            } else if (!db.commit()) {
                databaseFailed = true;
                result.status = Status::Failed;
                result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1")
                                          .arg(db.lastError().text());
                db.rollback();
            }
        }
    }
    // Con la conexión ya soltada, una que falló se descarta para que el
    // reintento abra otra. Un conflicto no dice nada de ella.
    if (databaseFailed)
        m_pool.discardThreadConnection();
    return result;
}
