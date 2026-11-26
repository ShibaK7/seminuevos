// Configuraciones (US-07.1) con sus widgets reales, sus presenters y los
// servicios sobre puertos en memoria: lo que las vistas falsas no ven (que
// bind() conecte, que los campos de verdad queden de solo lectura, el "Sin
// capturar" de la UMA) y su lugar en el menú.

#include "application/settings/services/systemsettingsservice.h"
#include "application/settings/services/useradministrationservice.h"
#include "fakes.h"
#include "presentation/settings/settingsview.h"
#include "presentation/settings/systemsettingspresenter.h"
#include "presentation/settings/systemsettingswidget.h"
#include "presentation/settings/usermanagementpresenter.h"
#include "presentation/settings/usermanagementwidget.h"
#include "presentation/shell/mainwindow.h"
#include "settingsfakes.h"
#include "userfakes.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QtTest>

#include <memory>

using domain::SystemParameter;
using domain::UserRole;

class TstSettingsView : public QObject
{
    Q_OBJECT

private:
    fakes::InMemorySettingsRepository m_settings;
    fakes::InMemoryUserAccountRepository m_accounts;
    fakes::FakePasswordHasher m_hasher;
    fakes::InlineTaskRunner m_runner;
    std::unique_ptr<application::SystemSettingsService> m_systemSettings;
    std::unique_ptr<application::UserAdministrationService> m_users;
    std::unique_ptr<SettingsView> m_view;
    int m_adminId = -1;

    // Arma la página como la raíz de composición.
    void open()
    {
        m_view = std::make_unique<SettingsView>();
        auto *parameters = new presentation::SystemSettingsPresenter(
            m_view->systemSettings(), *m_systemSettings, m_runner,
            {QStringLiteral("admin"), QStringLiteral("Admin"), UserRole::Admin,
             fakes::InMemorySettingsRepository::kAdministratorId},
            m_view.get());
        auto *users = new presentation::UserManagementPresenter(
            m_view->userManagement(), *m_users, m_runner,
            {QStringLiteral("admin"), QStringLiteral("Admin"), UserRole::Admin, m_adminId}, m_view.get());
        m_view->bind(*parameters, *users);
        parameters->start();
        users->start();
        m_view->show();
    }

    template <class T>
    T *find(const char *name) const
    {
        return m_view->findChild<T *>(QString::fromLatin1(name));
    }

private slots:
    void init()
    {
        m_settings = fakes::InMemorySettingsRepository();
        m_settings.store(SystemParameter::UmaDailyValue, 11731);
        m_settings.store(SystemParameter::BankBalance, 5000000);
        m_accounts = fakes::InMemoryUserAccountRepository();
        m_adminId = m_accounts.add(QStringLiteral("admin"), UserRole::Admin);
        m_accounts.add(QStringLiteral("vendedor"), UserRole::SalesAgent);
        m_systemSettings = std::make_unique<application::SystemSettingsService>(m_settings);
        m_users = std::make_unique<application::UserAdministrationService>(m_accounts, m_hasher);
    }

    void cleanup() { m_view.reset(); }

    // De solo lectura hasta "Editar", con lo guardado y su fecha.
    void opensReadOnlyWithTheStoredValues()
    {
        open();
        auto *uma = find<QDoubleSpinBox>("umaSpin");
        QVERIFY(uma->isReadOnly());
        QCOMPARE(uma->value(), 117.31);
        QCOMPARE(find<QSpinBox>("cashLimitSpin")->value(), 3210);
        QVERIFY(find<QLabel>("umaUpdated")->text().startsWith(QStringLiteral("Actualizado el ")));
        QCOMPARE(find<QLabel>("safeUpdated")->text(), QStringLiteral("Sin configurar"));
        QVERIFY(find<QPushButton>("editButton")->isVisible());
        QVERIFY(!find<QPushButton>("saveButton")->isVisible());
    }

    // La fecha llega en UTC (así la guarda la base) y se muestra en la hora
    // de la computadora: un cambio de noche no sale con la fecha de mañana.
    void updatedAtIsShownInLocalTime()
    {
        const QDateTime utc(QDate(2026, 11, 26), QTime(3, 0), QTimeZone::UTC);
        m_settings.store(SystemParameter::UmaDailyValue, 11731, utc);
        open();
        const QString expected =
            QLocale(QLocale::Spanish, QLocale::Mexico)
                .toString(utc.toLocalTime().date(), QStringLiteral("d 'de' MMMM 'de' yyyy"));
        QCOMPARE(find<QLabel>("umaUpdated")->text(), QStringLiteral("Actualizado el %1").arg(expected));
    }

    // "Editar" desbloquea; guardar escribe solo lo que cambió y vuelve a
    // consulta con la fecha nueva.
    void editAndSave()
    {
        open();
        find<QPushButton>("editButton")->click();
        auto *bank = find<QDoubleSpinBox>("bankSpin");
        QVERIFY(!bank->isReadOnly());
        QVERIFY(find<QPushButton>("saveButton")->isVisible());
        bank->setValue(60000.50);
        find<QPushButton>("saveButton")->click();
        QCOMPARE(m_settings.updates.size(), 1);
        QCOMPARE(m_settings.updates.first().size(), 1);
        QCOMPARE(m_settings.updates.first().first().value, qint64(6000050));
        QVERIFY(bank->isReadOnly());
        QVERIFY(find<QLabel>("bankUpdated")->text().contains(QStringLiteral("2026")));
    }

    // Un tope con decimales (capturado a mano en la base) se ve entero en el
    // campo; si nadie lo toca, no se reescribe al guardar otro parámetro.
    void untouchedFieldKeepsItsStoredValue()
    {
        m_settings.store(SystemParameter::CashLimitUmaMultiple, 320050);
        open();
        QCOMPARE(find<QSpinBox>("cashLimitSpin")->value(), 3200);
        find<QPushButton>("editButton")->click();
        find<QDoubleSpinBox>("bankSpin")->setValue(60000.0);
        find<QPushButton>("saveButton")->click();
        QCOMPARE(m_settings.updates.size(), 1);
        QCOMPARE(m_settings.updates.first().size(), 1);
        QCOMPARE(m_settings.updates.first().first().parameter, SystemParameter::BankBalance);
        QCOMPARE(m_settings.rows.value(SystemParameter::CashLimitUmaMultiple).hundredths,
                 std::optional<qint64>(320050));
    }

    // Mientras guarda, los campos no aceptan nada: se perdería al volver a
    // consulta.
    void fieldsAreReadOnlyWhileSaving()
    {
        open();
        find<QPushButton>("editButton")->click();
        auto *widget = m_view->findChild<SystemSettingsWidget *>();
        widget->setBusy(true);
        QVERIFY(find<QDoubleSpinBox>("safeSpin")->isReadOnly());
        widget->setBusy(false);
        QVERIFY(!find<QDoubleSpinBox>("safeSpin")->isReadOnly());
    }

    // En la UMA, el cero es "Sin capturar": no se guarda como cero.
    void emptyUmaIsRejected()
    {
        open();
        find<QPushButton>("editButton")->click();
        find<QDoubleSpinBox>("umaSpin")->setValue(0.0);
        QCOMPARE(find<QDoubleSpinBox>("umaSpin")->text(), QStringLiteral("Sin capturar"));
        find<QPushButton>("saveButton")->click();
        QCOMPARE(m_settings.updates.size(), 0);
        QVERIFY(!find<QDoubleSpinBox>("umaSpin")->isReadOnly());
        QCOMPARE(find<QDoubleSpinBox>("umaSpin")->property("hasError").toBool(), true);
    }

    // La tabla de usuarios marca tu cuenta y no te deja desactivarla.
    void usersTableMarksYourOwnAccount()
    {
        open();
        auto *table = find<QTableWidget>("usersTable");
        QCOMPARE(table->rowCount(), 2);
        int ownRow = -1;
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->item(row, UserManagementWidget::UsernameColumn)->text() == QStringLiteral("admin"))
                ownRow = row;
        }
        QVERIFY(ownRow >= 0);
        QVERIFY(table->item(ownRow, UserManagementWidget::NameColumn)->text().endsWith(QStringLiteral("(tú)")));
        table->selectRow(ownRow);
        QVERIFY(!find<QPushButton>("toggleActiveButton")->isEnabled());
        QVERIFY(find<QPushButton>("editUserButton")->isEnabled());
        table->selectRow(ownRow == 0 ? 1 : 0);
        QVERIFY(find<QPushButton>("toggleActiveButton")->isEnabled());
    }

    // Sin la página, el menú no muestra Configuraciones; con ella, sí, y
    // lleva a la página.
    void sidebarShowsSettingsOnlyWithItsPage()
    {
        MainWindow window;
        auto *item = window.findChild<QPushButton *>(QStringLiteral("settingsItem"));
        QVERIFY(item);
        QVERIFY(!item->isVisibleTo(&window));

        auto *page = new SettingsView;
        window.setSettingsPage(page);
        QVERIFY(item->isVisibleTo(&window));
        item->click();
        QCOMPARE(window.findChild<QStackedWidget *>(QStringLiteral("mainContentStack"))->currentWidget(),
                 page);
    }
};

QTEST_MAIN(TstSettingsView)
#include "tst_settingsview.moc"
