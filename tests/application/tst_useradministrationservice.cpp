// Administrar las cuentas de usuario desde Configuraciones (US-07.1): solo el
// Administrador, con las reglas del dominio antes de tocar la base, la
// contraseña siempre como hash, y lo que la base rechaza en el campo que toca.

#include "application/settings/services/useradministrationservice.h"
#include "fakes.h"
#include "userfakes.h"

#include <QtTest>

using application::SessionDto;
using domain::UserRole;
using Status = application::UserAccountWriteResult::Status;

class TstUserAdministrationService : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryUserAccountRepository m_accounts;
    fakes::FakePasswordHasher m_hasher;
    int m_adminId = -1;
    int m_sellerId = -1;

    application::UserAdministrationService service()
    {
        return application::UserAdministrationService(m_accounts, m_hasher);
    }

    SessionDto admin() const { return SessionDto{QStringLiteral("admin"), {}, UserRole::Admin, m_adminId}; }
    SessionDto seller() const
    {
        return SessionDto{QStringLiteral("vendedor"), {}, UserRole::SalesAgent, m_sellerId};
    }

    static application::NewUserAccountDto newUser()
    {
        return {QStringLiteral(" Ana.Lopez "), QStringLiteral(" Ana López "),
                QStringLiteral("clave segura"), QStringLiteral("clave segura"), UserRole::SalesAgent};
    }

private slots:
    void init()
    {
        m_accounts = fakes::InMemoryUserAccountRepository();
        m_adminId = m_accounts.add(QStringLiteral("admin"), UserRole::Admin);
        m_sellerId = m_accounts.add(QStringLiteral("vendedor"), UserRole::SalesAgent);
    }

    // La lista dice de cada cuenta qué se le puede cambiar.
    void listMarksWhatCanChange()
    {
        const auto result = service().listUsers(admin());
        QCOMPARE(result.status, application::UserAccountListResult::Status::Loaded);
        QCOMPARE(result.users.size(), 2);
        for (const application::UserAccountDto &user : result.users) {
            const bool self = user.id == m_adminId;
            QCOMPARE(user.isSelf, self);
            QCOMPARE(user.canChangeRole, !self);
            QCOMPARE(user.canChangeActive, !self);
        }
    }

    void sellerIsDeniedEverything()
    {
        QCOMPARE(service().listUsers(seller()).status, application::UserAccountListResult::Status::Denied);
        QCOMPARE(service().createUser(newUser(), seller()).status, Status::Denied);
        QCOMPARE(service().setActive(m_adminId, QStringLiteral("v1"), false, seller()).status,
                 Status::Denied);
        QCOMPARE(m_accounts.writes, 0);
    }

    // El usuario se guarda normalizado, el nombre sin espacios alrededor y la
    // contraseña como hash, nunca en claro.
    void createStoresANormalizedAccountWithAHash()
    {
        const auto result = service().createUser(newUser(), admin());
        QCOMPARE(result.status, Status::Saved);
        const auto row = m_accounts.rows.value(result.id);
        QCOMPARE(row.account.username, QStringLiteral("ana.lopez"));
        QCOMPARE(row.account.displayName, QStringLiteral("Ana López"));
        QCOMPARE(row.passwordHash, QStringLiteral("plain:clave segura"));
    }

    void invalidAccountIsRejectedWithoutWriting()
    {
        application::NewUserAccountDto user = newUser();
        user.username = QStringLiteral("a b");
        user.displayName.clear();
        user.passwordConfirmation = QStringLiteral("otra");
        const auto result = service().createUser(user, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(),
                 (QStringList{QStringLiteral("username"), QStringLiteral("displayName"),
                              QStringLiteral("passwordConfirmation")}));
        QCOMPARE(m_accounts.writes, 0);
    }

    // "VENDEDOR" ya existe como "vendedor": se marca en el campo.
    void duplicateUsernameIsReportedOnTheField()
    {
        application::NewUserAccountDto user = newUser();
        user.username = QStringLiteral("VENDEDOR");
        const auto result = service().createUser(user, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("username")});
    }

    void updateChangesNameAndRole()
    {
        const auto result = service().updateUser(
            {m_sellerId, QStringLiteral("v1"), QStringLiteral(" Juan "), UserRole::Admin}, admin());
        QCOMPARE(result.status, Status::Saved);
        QCOMPARE(m_accounts.rows.value(m_sellerId).account.displayName, QStringLiteral("Juan"));
        QCOMPARE(m_accounts.rows.value(m_sellerId).account.role, UserRole::Admin);
    }

    // Nadie se cambia su propio rol; su nombre sí.
    void ownRoleCannotChange()
    {
        const auto denied = service().updateUser(
            {m_adminId, QStringLiteral("v1"), QStringLiteral("Yo"), UserRole::SalesAgent}, admin());
        QCOMPARE(denied.status, Status::Rejected);
        QCOMPARE(denied.validation.fields(), QStringList{QStringLiteral("role")});
        QCOMPARE(m_accounts.writes, 0);

        const auto renamed = service().updateUser(
            {m_adminId, QStringLiteral("v1"), QStringLiteral("Yo"), UserRole::Admin}, admin());
        QCOMPARE(renamed.status, Status::Saved);
    }

    void ownAccountCannotBeDeactivated()
    {
        const auto result = service().setActive(m_adminId, QStringLiteral("v1"), false, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("active")});
    }

    void deactivateAndReactivate()
    {
        QCOMPARE(service().setActive(m_sellerId, QStringLiteral("v1"), false, admin()).status, Status::Saved);
        QVERIFY(!m_accounts.rows.value(m_sellerId).account.active);
        const QString version = m_accounts.rows.value(m_sellerId).account.version;
        QCOMPARE(service().setActive(m_sellerId, version, true, admin()).status, Status::Saved);
        QVERIFY(m_accounts.rows.value(m_sellerId).account.active);
    }

    // Lo que la base decide llega a la pantalla con su sentido: un conflicto,
    // una cuenta que ya no existe, y el último Administrador activo.
    void databaseOutcomesAreTranslated()
    {
        QCOMPARE(service().setActive(m_sellerId, QStringLiteral("vieja"), false, admin()).status,
                 Status::Conflict);
        QCOMPARE(service().setActive(99, QStringLiteral("v1"), false, admin()).status, Status::NotFound);
    }

    // Otro Administrador le quitó el rol (o lo desactivó) después de que abrió
    // la pantalla: su sesión ya no basta.
    void actorWhoIsNoLongerAnAdministratorIsDenied()
    {
        m_accounts.rows[m_adminId].account.role = UserRole::SalesAgent;
        QCOMPARE(service().createUser(newUser(), admin()).status, Status::Denied);
        QCOMPARE(service().setActive(m_sellerId, QStringLiteral("v1"), false, admin()).status,
                 Status::Denied);
    }

    // Que quede un Administrador activo lo cuida la base, aunque por las
    // reglas nadie llegue ahí: si lo rechaza, se marca en el campo que toca.
    void lastActiveAdministratorIsReportedOnTheField()
    {
        m_accounts.forced = fakes::InMemoryUserAccountRepository::Outcome::Status::LastActiveAdmin;
        const auto deactivated = service().setActive(m_sellerId, QStringLiteral("v1"), false, admin());
        QCOMPARE(deactivated.status, Status::Rejected);
        QCOMPARE(deactivated.validation.fields(), QStringList{QStringLiteral("active")});
        const auto demoted = service().updateUser(
            {m_sellerId, QStringLiteral("v1"), QStringLiteral("Juan"), UserRole::SalesAgent}, admin());
        QCOMPARE(demoted.validation.fields(), QStringList{QStringLiteral("role")});
        QVERIFY(demoted.validation.joinedMessages().contains(QStringLiteral("Administrador activo")));
    }

    void passwordResetStoresAHash()
    {
        QCOMPARE(service().resetPassword({m_sellerId, QStringLiteral("nueva clave"),
                                          QStringLiteral("nueva clave")},
                                         admin())
                     .status,
                 Status::Saved);
        QCOMPARE(m_accounts.rows.value(m_sellerId).passwordHash, QStringLiteral("plain:nueva clave"));
        const auto weak = service().resetPassword({m_sellerId, QStringLiteral("corta"),
                                                   QStringLiteral("corta")},
                                                  admin());
        QCOMPARE(weak.status, Status::Rejected);
    }

    void failedDatabaseIsReported()
    {
        m_accounts.failure = QStringLiteral("sin conexión");
        const auto list = service().listUsers(admin());
        QCOMPARE(list.status, application::UserAccountListResult::Status::Failed);
        const auto created = service().createUser(newUser(), admin());
        QCOMPARE(created.status, Status::Failed);
        QCOMPARE(created.errorMessage, QStringLiteral("sin conexión"));
    }
};

QTEST_APPLESS_MAIN(TstUserAdministrationService)
#include "tst_useradministrationservice.moc"
