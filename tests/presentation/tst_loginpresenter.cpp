// Pruebas del presenter de inicio de sesión con una vista falsa y un
// TaskRunner inmediato: se prueba la lógica de la pantalla sin abrir ventanas
// ni hilos.

#include "application/services/authenticationservice.h"
#include "fakes.h"
#include "presentation/presenters/loginpresenter.h"

#include <QSignalSpy>
#include <QtTest>

class TstLoginPresenter : public QObject
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

    void successEmitsTheSessionAndReleasesTheView()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        fakes::FakeLoginView view;
        fakes::InlineTaskRunner runner;
        presentation::LoginPresenter presenter(view, auth, runner);
        QSignalSpy spy(&presenter, &presentation::LoginPresenter::authenticated);

        presenter.login(QStringLiteral("ana"), QStringLiteral("secreta"));

        QCOMPARE(spy.count(), 1);
        const auto session = spy.at(0).at(0).value<application::SessionDto>();
        QCOMPARE(session.role, QStringLiteral("Vendedor"));
        QCOMPARE(view.busyChanges, (QList<bool>{true, false}));
        QVERIFY(view.errors.isEmpty());
    }

    void failureShowsTheErrorAndDoesNotAuthenticate()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        fakes::FakeLoginView view;
        fakes::InlineTaskRunner runner;
        presentation::LoginPresenter presenter(view, auth, runner);
        QSignalSpy spy(&presenter, &presentation::LoginPresenter::authenticated);

        presenter.login(QStringLiteral("ana"), QStringLiteral("otra"));

        QCOMPARE(spy.count(), 0);
        QCOMPARE(view.errors.size(), 1);
        QCOMPARE(view.busyChanges, (QList<bool>{true, false}));
    }

    // Tras un fallo se puede volver a intentar: el presenter no queda trabado.
    void canRetryAfterAFailure()
    {
        const application::AuthenticationService auth(m_users, m_hasher);
        fakes::FakeLoginView view;
        fakes::InlineTaskRunner runner;
        presentation::LoginPresenter presenter(view, auth, runner);
        QSignalSpy spy(&presenter, &presentation::LoginPresenter::authenticated);

        presenter.login(QStringLiteral("ana"), QStringLiteral("otra"));
        presenter.login(QStringLiteral("ana"), QStringLiteral("secreta"));

        QCOMPARE(runner.submitted, 2);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_APPLESS_MAIN(TstLoginPresenter)
#include "tst_loginpresenter.moc"
