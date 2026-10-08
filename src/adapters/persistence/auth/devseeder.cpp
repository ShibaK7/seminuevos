#include "adapters/persistence/auth/devseeder.h"
#include "application/auth/ports/passwordhasher.h"

#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

struct SeedUser
{
    QString username;
    QString plainPassword;
    QString displayName;
    QString role;
};

// El hash se calcula aquí, en el momento (con el PasswordHasher que se recibe), en vez
// de venir pegado como texto en un script SQL -- así agregar/cambiar un
// usuario de prueba es solo una línea de C++, sin herramientas aparte.
bool insertUser(QSqlDatabase &db, const SeedUser &user, const application::PasswordHasher &hasher,
                QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO users (username, password_hash, display_name, role) "
        "VALUES (:username, :password_hash, :display_name, :role) "
        "ON CONFLICT (username) DO NOTHING"));
    query.bindValue(QStringLiteral(":username"), user.username);
    query.bindValue(QStringLiteral(":password_hash"), hasher.hash(user.plainPassword));
    query.bindValue(QStringLiteral(":display_name"), user.displayName);
    query.bindValue(QStringLiteral(":role"), user.role);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

} // namespace

DevSeeder::Result DevSeeder::run(QSqlDatabase &db, const application::PasswordHasher &hasher)
{
    Result result;

    static const QList<SeedUser> seedUsers = {
        {QStringLiteral("a"), QStringLiteral("a"),
         QStringLiteral("Administrador"), QStringLiteral("Administrador")},
        {QStringLiteral("admin"), QStringLiteral("Admin123!"),
         QStringLiteral("Administrador"), QStringLiteral("Administrador")},
        {QStringLiteral("vendedor"), QStringLiteral("Vendedor123!"),
         QStringLiteral("Vendedor de Prueba"), QStringLiteral("Vendedor")},
    };

    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1").arg(db.lastError().text());
        return result;
    }

    for (const SeedUser &user : seedUsers) {
        QString errorMessage;
        if (!insertUser(db, user, hasher, errorMessage)) {
            result.errorMessage = QStringLiteral("Error insertando usuario semilla '%1': %2")
                                       .arg(user.username, errorMessage);
            db.rollback();
            return result;
        }
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1").arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    return result;
}
