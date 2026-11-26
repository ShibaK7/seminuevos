// El presenter de la gestión de usuarios (US-07.1) con una vista falsa y el
// servicio real sobre la tabla users en memoria: la lista, el formulario en
// sus tres modos, activar y desactivar, y lo que la base decide.

#include "application/settings/services/useradministrationservice.h"
#include "fakes.h"
#include "presentation/settings/iusermanagementview.h"
#include "presentation/settings/usermanagementpresenter.h"
#include "userfakes.h"

#include <QtTest>

using domain::UserRole;

namespace {

class FakeUserManagementView final : public presentation::IUserManagementView
{
public:
    bool busy = false;
    QList<application::UserAccountDto> users;
    int selected = -1;
    QString form; // "" si no hay, "alta", "edición" o "contraseña"
    application::UserAccountDto formAccount;
    bool formBusy = false;
    QList<domain::ValidationError> formErrors;
    bool confirm = true;
    int confirmations = 0;
    QString message;
    QString notice;

    void setBusy(bool value) override { busy = value; }
    void showUsers(const QList<application::UserAccountDto> &value) override { users = value; }
    int selectedUserId() const override { return selected; }
    void askNewUser() override { form = QStringLiteral("alta"); }
    void askUserEdit(const application::UserAccountDto &user) override
    {
        form = QStringLiteral("edición");
        formAccount = user;
    }
    void askPasswordReset(const application::UserAccountDto &user) override
    {
        form = QStringLiteral("contraseña");
        formAccount = user;
    }
    void setUserFormBusy(bool value) override { formBusy = value; }
    void showUserFormErrors(const QList<domain::ValidationError> &errors) override { formErrors = errors; }
    void closeUserForm() override { form.clear(); }
    bool confirmActivation(const application::UserAccountDto &, bool) override
    {
        ++confirmations;
        return confirm;
    }
    void showMessage(const QString &value) override { message = value; }
    void showNotice(const QString &value) override { notice = value; }

    application::UserAccountDto user(int id) const
    {
        for (const application::UserAccountDto &candidate : users) {
            if (candidate.id == id)
                return candidate;
        }
        return {};
    }
};

} // namespace

class TstUserManagementPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryUserAccountRepository m_accounts;
    fakes::FakePasswordHasher m_hasher;
    std::unique_ptr<application::UserAdministrationService> m_service;
    FakeUserManagementView m_view;
    fakes::InlineTaskRunner m_runner;
    int m_adminId = -1;
    int m_sellerId = -1;

    std::unique_ptr<presentation::UserManagementPresenter> make(presentation::TaskRunner &runner)
    {
        const application::SessionDto actor{QStringLiteral("admin"), {}, UserRole::Admin, m_adminId};
        auto presenter = std::make_unique<presentation::UserManagementPresenter>(m_view, *m_service,
                                                                                 runner, actor);
        presenter->start();
        return presenter;
    }

private slots:
    void init()
    {
        m_accounts = fakes::InMemoryUserAccountRepository();
        m_adminId = m_accounts.add(QStringLiteral("admin"), UserRole::Admin);
        m_sellerId = m_accounts.add(QStringLiteral("vendedor"), UserRole::SalesAgent);
        m_service = std::make_unique<application::UserAdministrationService>(m_accounts, m_hasher);
        m_view = FakeUserManagementView();
    }

    void listsTheAccountsMarkingYourOwn()
    {
        auto presenter = make(m_runner);
        QCOMPARE(m_view.users.size(), 2);
        QVERIFY(m_view.user(m_adminId).isSelf);
        QVERIFY(!m_view.user(m_sellerId).isSelf);
        QVERIFY(!m_view.busy);
    }

    // El alta guarda, cierra el formulario, avisa y relee la lista.
    void newAccountClosesTheFormAndReloads()
    {
        auto presenter = make(m_runner);
        presenter->onAddUser();
        QCOMPARE(m_view.form, QStringLiteral("alta"));
        presenter->onSubmitNewUser({QStringLiteral("ana"), QStringLiteral("Ana"), QStringLiteral("clave segura"),
                                    QStringLiteral("clave segura"), UserRole::SalesAgent});
        QVERIFY(m_view.form.isEmpty());
        QVERIFY(m_view.notice.contains(QStringLiteral("Ana")));
        QCOMPARE(m_view.users.size(), 3);
    }

    // Con errores, el formulario sigue abierto con ellos.
    void rejectedAccountKeepsTheFormWithItsErrors()
    {
        auto presenter = make(m_runner);
        presenter->onAddUser();
        presenter->onSubmitNewUser({QStringLiteral("VENDEDOR"), QStringLiteral("Otro"), QStringLiteral("clave segura"),
                                    QStringLiteral("clave segura"), UserRole::SalesAgent});
        QCOMPARE(m_view.form, QStringLiteral("alta"));
        QCOMPARE(m_view.formErrors.size(), 1);
        QCOMPARE(m_view.formErrors.first().field, QStringLiteral("username"));
        QVERIFY(!m_view.formBusy);
    }

    // Editar y la contraseña piden una cuenta seleccionada.
    void editAndPasswordNeedASelection()
    {
        auto presenter = make(m_runner);
        presenter->onEditUser();
        QVERIFY(m_view.form.isEmpty());
        QVERIFY(!m_view.message.isEmpty());
        m_view.selected = m_sellerId;
        presenter->onEditUser();
        QCOMPARE(m_view.form, QStringLiteral("edición"));
        QCOMPARE(m_view.formAccount.id, m_sellerId);
        presenter->onResetPassword();
        QCOMPARE(m_view.form, QStringLiteral("contraseña"));
    }

    void editSavesWithTheVersionThatWasRead()
    {
        auto presenter = make(m_runner);
        m_view.selected = m_sellerId;
        presenter->onEditUser();
        application::UserAccountUpdateDto update{m_sellerId, m_view.formAccount.version,
                                                 QStringLiteral("Juan"), UserRole::SalesAgent};
        presenter->onSubmitUserEdit(update);
        QVERIFY(m_view.form.isEmpty());
        QCOMPARE(m_view.user(m_sellerId).displayName, QStringLiteral("Juan"));

        // La misma versión ya es vieja: conflicto, se cierra y se relee.
        presenter->onEditUser();
        presenter->onSubmitUserEdit(update);
        QVERIFY(m_view.form.isEmpty());
        QVERIFY(m_view.message.contains(QStringLiteral("Otro administrador")));
    }

    // Desactivar pregunta antes; sin confirmar, no hace nada.
    void deactivateAsksFirst()
    {
        auto presenter = make(m_runner);
        m_view.selected = m_sellerId;
        m_view.confirm = false;
        presenter->onToggleActive();
        QCOMPARE(m_view.confirmations, 1);
        QCOMPARE(m_accounts.writes, 0);
        m_view.confirm = true;
        presenter->onToggleActive();
        QVERIFY(!m_view.user(m_sellerId).active);
        QVERIFY(m_view.notice.contains(QStringLiteral("desactivó")));
        // La siguiente vez la reactiva.
        presenter->onToggleActive();
        QVERIFY(m_view.user(m_sellerId).active);
    }

    // La propia cuenta no se desactiva, ni se pregunta.
    void ownAccountIsNotDeactivated()
    {
        auto presenter = make(m_runner);
        m_view.selected = m_adminId;
        presenter->onToggleActive();
        QCOMPARE(m_view.confirmations, 0);
        QVERIFY(m_view.message.contains(QStringLiteral("propia")));
        QCOMPARE(m_accounts.writes, 0);
    }

    // Si la base falla, el formulario se queda con lo capturado.
    void failedSaveKeepsTheForm()
    {
        auto presenter = make(m_runner);
        presenter->onAddUser();
        m_accounts.failure = QStringLiteral("sin conexión");
        presenter->onSubmitNewUser({QStringLiteral("ana"), QStringLiteral("Ana"), QStringLiteral("clave segura"),
                                    QStringLiteral("clave segura"), UserRole::SalesAgent});
        QCOMPARE(m_view.form, QStringLiteral("alta"));
        QVERIFY(m_view.message.contains(QStringLiteral("sin conexión")));
    }

    // Mientras guarda no se aceptan más clics ni otra escritura.
    void oneWriteAtATime()
    {
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        deferred.finish(0);
        presenter->onAddUser();
        presenter->onSubmitNewUser({QStringLiteral("ana"), QStringLiteral("Ana"), QStringLiteral("clave segura"),
                                    QStringLiteral("clave segura"), UserRole::SalesAgent});
        QVERIFY(m_view.formBusy);
        presenter->onSubmitNewUser({QStringLiteral("beto"), QStringLiteral("Beto"), QStringLiteral("clave segura"),
                                    QStringLiteral("clave segura"), UserRole::SalesAgent});
        QCOMPARE(deferred.works.size(), 2); // la lista y una sola escritura
        deferred.finish(1);
        QVERIFY(!m_view.formBusy);
    }
};

QTEST_GUILESS_MAIN(TstUserManagementPresenter)
#include "tst_usermanagementpresenter.moc"
