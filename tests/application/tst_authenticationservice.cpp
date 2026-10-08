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
        QCOMPARE(result.session.role, QStringLiteral("Vendedor"));
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
