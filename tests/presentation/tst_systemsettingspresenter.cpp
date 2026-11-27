// El presenter de los parámetros de Configuraciones (US-07.1, #82) con una
// vista falsa y el servicio real sobre global_configurations en memoria:
// solo lectura hasta "Editar", guardar solo lo que cambió, cancelar, los
// conflictos y salir con cambios sin guardar.

#include "application/settings/services/systemsettingsservice.h"
#include "fakes.h"
#include "presentation/settings/isystemsettingsview.h"
#include "presentation/settings/systemsettingspresenter.h"
#include "settingsfakes.h"

#include <QtTest>

using domain::SystemParameter;
using presentation::FormMode;

namespace {

class FakeSystemSettingsView final : public presentation::ISystemSettingsView
{
public:
    bool busy = false;
    QList<application::SystemParameterDto> shown;
    QMap<SystemParameter, std::optional<qint64>> fields;
    FormMode mode = FormMode::View;
    bool canEdit = false;
    QList<domain::ValidationError> errors;
    QString message;
    QString notice;
    bool discard = true;
    int discardQuestions = 0;
    int paints = 0;

    void setBusy(bool value) override { busy = value; }
    void showParameters(const QList<application::SystemParameterDto> &parameters) override
    {
        ++paints;
        shown = parameters;
        for (const application::SystemParameterDto &dto : parameters)
            fields[dto.parameter] = dto.hundredths ? dto.hundredths : domain::defaultHundredths(dto.parameter);
    }
    QList<EditedValue> editedValues() const override
    {
        QList<EditedValue> values;
        for (auto it = fields.cbegin(); it != fields.cend(); ++it)
            values << EditedValue{it.key(), it.value()};
        return values;
    }
    void showMode(FormMode value, bool editable) override
    {
        mode = value;
        canEdit = editable;
    }
    void showFieldErrors(const QList<domain::ValidationError> &value) override { errors = value; }
    void showMessage(const QString &value) override { message = value; }
    void showNotice(const QString &value) override { notice = value; }
    bool confirmDiscardChanges() override
    {
        ++discardQuestions;
        return discard;
    }
};

} // namespace

class TstSystemSettingsPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemorySettingsRepository m_settings;
    std::unique_ptr<application::SystemSettingsService> m_service;
    FakeSystemSettingsView m_view;
    fakes::InlineTaskRunner m_runner;

    std::unique_ptr<presentation::SystemSettingsPresenter> make(presentation::TaskRunner &runner,
                                                               domain::UserRole role = domain::UserRole::Admin)
    {
        const application::SessionDto actor{QStringLiteral("admin"), QStringLiteral("Admin"), role,
                                            fakes::InMemorySettingsRepository::kAdministratorId};
        auto presenter = std::make_unique<presentation::SystemSettingsPresenter>(m_view, *m_service,
                                                                                 runner, actor);
        presenter->start();
        return presenter;
    }

    // Lo que hace la vista cuando el usuario cambia un campo.
    void type(presentation::SystemSettingsPresenter &presenter, SystemParameter parameter, qint64 value)
    {
        m_view.fields[parameter] = value;
        presenter.onEdited();
    }

private slots:
    void init()
    {
        m_settings = fakes::InMemorySettingsRepository();
        m_settings.store(SystemParameter::UmaDailyValue, 11731);
        m_settings.store(SystemParameter::CashLimitUmaMultiple, 321000);
        m_service = std::make_unique<application::SystemSettingsService>(m_settings);
        m_view = FakeSystemSettingsView();
    }

    // Abre de solo lectura, con los dos pintados y "Editar" disponible.
    void opensReadOnlyWithTheParameters()
    {
        auto presenter = make(m_runner);
        QCOMPARE(m_view.mode, FormMode::View);
        QVERIFY(m_view.canEdit);
        QCOMPARE(m_view.shown.size(), 2);
        QVERIFY(!m_view.busy);
    }

    // Mientras no llega la lectura no se puede editar.
    void editWaitsForTheFirstRead()
    {
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        QVERIFY(m_view.busy);
        presenter->onEdit();
        QCOMPARE(m_view.mode, FormMode::View);
        deferred.finish(0);
        presenter->onEdit();
        QCOMPARE(m_view.mode, FormMode::Edit);
    }

    // Guardar manda solo lo que cambió y vuelve a consulta con la fecha nueva.
    void saveSendsOnlyWhatChanged()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        type(*presenter, SystemParameter::UmaDailyValue, 12000);
        presenter->onSave();
        QCOMPARE(m_settings.updates.size(), 1);
        QCOMPARE(m_settings.updates.first().size(), 1);
        QCOMPARE(m_settings.updates.first().first().parameter, SystemParameter::UmaDailyValue);
        QCOMPARE(m_settings.updates.first().first().expected, std::optional<qint64>(11731));
        QCOMPARE(m_view.mode, FormMode::View);
        QCOMPARE(m_view.notice, QStringLiteral("Cambios guardados."));
        // Se releyó: la UMA ya trae la fecha del guardado.
        for (const application::SystemParameterDto &dto : m_view.shown) {
            if (dto.parameter == SystemParameter::UmaDailyValue)
                QCOMPARE(dto.updatedAt, m_settings.now);
        }
    }

    // Sin cambios no se toca la base.
    void saveWithoutChangesOnlyLeavesEditMode()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        presenter->onSave();
        QCOMPARE(m_settings.updates.size(), 0);
        QCOMPARE(m_view.mode, FormMode::View);
        QCOMPARE(m_view.notice, QStringLiteral("No hubo cambios que guardar."));
    }

    // Volver a la página a media edición no relee ni pisa lo capturado.
    void refreshMidEditKeepsTheEdits()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 100);
        const int reads = m_settings.reads;
        const int paints = m_view.paints;
        presenter->refresh();
        QCOMPARE(m_settings.reads, reads);
        QCOMPARE(m_view.paints, paints);
        QCOMPARE(m_view.fields.value(SystemParameter::CashLimitUmaMultiple), std::optional<qint64>(100));
    }

    void invalidValueStaysInEditWithItsError()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 500000); // 5,000 UMA
        presenter->onSave();
        QCOMPARE(m_view.mode, FormMode::Edit);
        QCOMPARE(m_view.errors.size(), 1);
        QCOMPARE(m_view.errors.first().field, QStringLiteral("cashLimitUmaMultiple"));
        QCOMPARE(m_settings.updates.size(), 0);
    }

    // Cancelar con cambios pregunta; si no acepta perderlos, sigue editando.
    void cancelAsksBeforeDiscarding()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 100);
        m_view.discard = false;
        presenter->onCancel();
        QCOMPARE(m_view.mode, FormMode::Edit);
        m_view.discard = true;
        presenter->onCancel();
        QCOMPARE(m_view.mode, FormMode::View);
        // Se repinta lo leído.
        QCOMPARE(m_view.fields.value(SystemParameter::CashLimitUmaMultiple), std::optional<qint64>(321000));
    }

    // Otro administrador lo cambió: no se pisa, se avisa y se carga lo vigente.
    void conflictReloadsTheCurrentValues()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        m_settings.store(SystemParameter::UmaDailyValue, 11800);
        type(*presenter, SystemParameter::UmaDailyValue, 12000);
        presenter->onSave();
        QCOMPARE(m_view.mode, FormMode::View);
        QVERIFY(m_view.message.contains(QStringLiteral("Otro administrador")));
        QCOMPARE(m_view.fields.value(SystemParameter::UmaDailyValue), std::optional<qint64>(11800));
    }

    // Lo guardado se ve en cuanto se guarda, sin esperar la relectura; y si
    // esa relectura falla, el siguiente guardado espera lo que se guardó, no
    // lo de antes (no es un conflicto consigo mismo).
    void savedValuesShowRightAwayAndAreTheNextExpected()
    {
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        deferred.finish(0);
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 200000);
        presenter->onSave();
        deferred.finish(1);
        QCOMPARE(m_view.notice, QStringLiteral("Cambios guardados."));
        QCOMPARE(m_view.fields.value(SystemParameter::CashLimitUmaMultiple), std::optional<qint64>(200000));

        m_settings.failure = QStringLiteral("se cayó la base");
        deferred.finish(2); // la relectura falla
        QCOMPARE(m_view.fields.value(SystemParameter::CashLimitUmaMultiple), std::optional<qint64>(200000));

        m_settings.failure.clear();
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 150000);
        presenter->onSave();
        deferred.finish(3);
        QCOMPARE(m_settings.rows.value(SystemParameter::CashLimitUmaMultiple).hundredths,
                 std::optional<qint64>(150000));
        QVERIFY(!m_view.message.contains(QStringLiteral("Otro administrador")));
    }

    // Un error de lectura se va cuando una lectura posterior sale bien.
    void successfulReadClearsAnOldError()
    {
        m_settings.failure = QStringLiteral("se cayó la base");
        auto presenter = make(m_runner);
        QVERIFY(m_view.message.contains(QStringLiteral("se cayó la base")));
        m_settings.failure.clear();
        presenter->refresh();
        QVERIFY(m_view.message.isEmpty());
    }

    // Si falla la base, se queda en edición con lo capturado.
    void failedSaveKeepsTheEdits()
    {
        auto presenter = make(m_runner);
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 100000);
        m_settings.failure = QStringLiteral("se cayó la base");
        presenter->onSave();
        QCOMPARE(m_view.mode, FormMode::Edit);
        QVERIFY(m_view.message.contains(QStringLiteral("se cayó la base")));
        QCOMPARE(m_view.fields.value(SystemParameter::CashLimitUmaMultiple), std::optional<qint64>(100000));
    }

    // Salir con cambios pregunta; sin cambios, no. Mientras guarda, espera.
    void leavingAsksOnlyWithUnsavedChanges()
    {
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        deferred.finish(0);
        QVERIFY(presenter->confirmLeave());
        QCOMPARE(m_view.discardQuestions, 0);

        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 100);
        m_view.discard = false;
        QVERIFY(!presenter->confirmLeave());
        presenter->onSave();
        QVERIFY(!presenter->confirmLeave()); // guardando
        deferred.finish(1);
        QVERIFY(presenter->confirmLeave());
    }

    // Una lectura vieja que llega después de una nueva no se pinta.
    void staleReadIsDiscarded()
    {
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        presenter->refresh();
        m_settings.store(SystemParameter::UmaDailyValue, 13000);
        deferred.finish(1);
        const int paints = m_view.paints;
        m_settings.store(SystemParameter::UmaDailyValue, 11731);
        deferred.finish(0);
        QCOMPARE(m_view.paints, paints);
        QCOMPARE(m_view.fields.value(SystemParameter::UmaDailyValue), std::optional<qint64>(13000));
    }

    // Le quitaron el rol con la sesión abierta: no se guarda y ya no se
    // ofrece editar.
    void administratorDemotedMeanwhileIsDenied()
    {
        auto presenter = make(m_runner);
        m_settings.activeAdministrators.clear();
        presenter->onEdit();
        type(*presenter, SystemParameter::CashLimitUmaMultiple, 200000);
        presenter->onSave();
        QCOMPARE(m_settings.rows.value(SystemParameter::CashLimitUmaMultiple).hundredths,
                 std::optional<qint64>(321000));
        QCOMPARE(m_view.mode, FormMode::View);
        QVERIFY(!m_view.canEdit);
        QVERIFY(m_view.message.contains(QStringLiteral("ya no puede")));
    }

    void sellerCannotEdit()
    {
        auto presenter = make(m_runner, domain::UserRole::SalesAgent);
        QVERIFY(!m_view.canEdit);
        QVERIFY(!m_view.message.isEmpty());
        presenter->onEdit();
        QCOMPARE(m_view.mode, FormMode::View);
    }
};

QTEST_GUILESS_MAIN(TstSystemSettingsPresenter)
#include "tst_systemsettingspresenter.moc"
