// Pruebas del caso de uso "iniciar sesión" con puertos falsos: sin base de
// datos y sin las 210,000 iteraciones del hash real.

#include "application/auth/services/authenticationservice.h"
#include "fakes.h"

#include <QtTest>

class TstAuthenticationService : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryUserDirectory m_users;
    fakes::FakePasswordHasher m_hasher;

private slots:
    void init()
    {
        m_users = fakes::InMemoryUserDirectory();
        m_users.users.insert(QStringLiteral("ana"),
                             {m_hasher.hash(QStringLiteral("secreta")),
                              QStringLiteral("Ana López"), QStringLiteral("Vendedor")});
    }

    void validCredentialsOpenASession()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        const application::LoginResult result = auth.login(QStringLiteral("ana"), QStringLiteral("secreta"));
        QVERIFY(result.ok);
        QCOMPARE(result.session.username, QStringLiteral("ana"));
        QCOMPARE(result.session.displayName, QStringLiteral("Ana López"));
<<<<<<< HEAD
        QCOMPARE(result.session.role, QStringLiteral("Vendedor"));
        QCOMPARE(m_users.successfulLogins, 1);
=======
        QCOMPARE(result.session.role, domain::UserRole::SalesAgent);
    }

    // El texto de la base se traduce al rol del dominio: nadie más compara
    // "Administrador" a mano.
    void theRoleComesTranslated()
    {
        m_users.users.insert(QStringLiteral("root"),
                             {m_hasher.hash(QStringLiteral("clave")),
                              QStringLiteral("Dueño"), QStringLiteral("Administrador")});
        const application::AuthenticationService auth(m_users, m_hasher);
        const auto result = auth.login(QStringLiteral("root"), QStringLiteral("clave"));
        QVERIFY(result.ok);
        QCOMPARE(result.session.role, domain::UserRole::Admin);
    }

    // Un rol que la aplicación no conoce no entra: no se le adivinan permisos.
    void unknownRoleIsRejected()
    {
        m_users.users.insert(QStringLiteral("raro"),
                             {m_hasher.hash(QStringLiteral("clave")),
                              QStringLiteral("Raro"), QStringLiteral("administrador")});
        const application::AuthenticationService auth(m_users, m_hasher);
        const auto result = auth.login(QStringLiteral("raro"), QStringLiteral("clave"));
        QVERIFY(!result.ok);
        QVERIFY(result.errorMessage.contains(QStringLiteral("rol")));
>>>>>>> 1041035 (Carry the session role as a domain enum)
    }

    // "No existe" y "contraseña equivocada" dan el mismo mensaje: distinguirlos
    // le diría a un tercero qué usuarios existen.
    void wrongPasswordAndUnknownUserLookTheSame()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        const auto wrongPassword = auth.login(QStringLiteral("ana"), QStringLiteral("otra"));
        const auto unknownUser = auth.login(QStringLiteral("nadie"), QStringLiteral("secreta"));
        QVERIFY(!wrongPassword.ok);
        QVERIFY(!unknownUser.ok);
        QCOMPARE(m_users.successfulLogins, 0);
        QCOMPARE(wrongPassword.errorMessage, unknownUser.errorMessage);
    }

    void emptyFieldsDoNotQueryTheDirectory()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        QVERIFY(!auth.login(QString(), QStringLiteral("x")).ok);
        QVERIFY(!auth.login(QStringLiteral("ana"), QString()).ok);
        QCOMPARE(m_users.lookups, 0);
    }

    void usernameIsTrimmed()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        QVERIFY(auth.login(QStringLiteral("  ana "), QStringLiteral("secreta")).ok);
    }

    // Una falla de la base no se disfraza de "contraseña incorrecta".
    void directoryFailureIsReportedAsSuch()
    {
        m_users.failWith = QStringLiteral("server closed the connection");
        const application::AuthenticationService auth(m_users, m_hasher);
        const auto result = auth.login(QStringLiteral("ana"), QStringLiteral("secreta"));
        QVERIFY(!result.ok);
        QVERIFY(result.errorMessage.contains(QStringLiteral("server closed the connection")));
    }
};

QTEST_APPLESS_MAIN(TstAuthenticationService)
#include "tst_authenticationservice.moc"
