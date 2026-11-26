// Consultar y corregir los parámetros de Configuraciones (US-07.1, #83 y #84):
// solo el Administrador, solo se guarda lo que cambió, y un cambio de otro
// administrador mientras tanto es un conflicto, no se pisa.

#include "application/settings/services/systemsettingsservice.h"
#include "settingsfakes.h"

#include <QtTest>

using application::SystemParameterEditDto;
using domain::SystemParameter;
using Status = application::SystemParametersUpdateResult::Status;

class TstSystemSettingsService : public QObject
{
    Q_OBJECT

private:
    fakes::InMemorySettingsRepository m_settings;

    application::SystemSettingsService service() { return application::SystemSettingsService(m_settings); }

    static application::SessionDto admin()
    {
        return {QStringLiteral("admin"), QStringLiteral("Admin"), domain::UserRole::Admin,
                fakes::InMemorySettingsRepository::kAdministratorId};
    }

    static application::SessionDto seller()
    {
        return {QStringLiteral("vendedor"), QStringLiteral("Vendedor"), domain::UserRole::SalesAgent, 2};
    }

    // Lo que la pantalla manda si nadie toca nada: lo leído, tal cual.
    QList<SystemParameterEditDto> untouched()
    {
        QList<SystemParameterEditDto> edits;
        const auto loaded = service().getSystemParameters(admin());
        for (const application::SystemParameterDto &dto : loaded.parameters) {
            const std::optional<qint64> shown =
                dto.hundredths ? dto.hundredths : domain::defaultHundredths(dto.parameter);
            edits << SystemParameterEditDto{dto.parameter, dto.hundredths, shown};
        }
        return edits;
    }

    static SystemParameterEditDto &edit(QList<SystemParameterEditDto> &edits, SystemParameter parameter)
    {
        for (SystemParameterEditDto &e : edits) {
            if (e.parameter == parameter)
                return e;
        }
        qFatal("parámetro sin campo");
    }

private slots:
    void init()
    {
        m_settings = fakes::InMemorySettingsRepository();
        m_settings.store(SystemParameter::UmaDailyValue, 11731);
        m_settings.store(SystemParameter::CashLimitUmaMultiple, 321000);
        m_settings.store(SystemParameter::BankBalance, 5000000);
    }

    // Los cinco, en orden; los que no tienen renglón vienen sin valor ni fecha.
    void loadsAllFiveInOrder()
    {
        const auto result = service().getSystemParameters(admin());
        QCOMPARE(result.status, application::SystemParametersResult::Status::Loaded);
        QCOMPARE(result.parameters.size(), 5);
        QCOMPARE(result.parameters.at(0).parameter, SystemParameter::UmaDailyValue);
        QCOMPARE(result.parameters.at(0).hundredths, std::optional<qint64>(11731));
        QVERIFY(result.parameters.at(0).updatedAt.isValid());
        QCOMPARE(result.parameters.at(4).parameter, SystemParameter::SafeBalance);
        QCOMPARE(result.parameters.at(4).hundredths, std::nullopt);
        QVERIFY(!result.parameters.at(4).updatedAt.isValid());
    }

    void sellerIsDeniedWithoutReading()
    {
        QCOMPARE(service().getSystemParameters(seller()).status,
                 application::SystemParametersResult::Status::Denied);
        QCOMPARE(service().updateSystemParameters(untouched(), seller()).status,
                 Status::Denied);
        QCOMPARE(m_settings.updates.size(), 0);
    }

    // La sesión dice Administrador, pero en la base ya no lo es: el
    // repositorio lo revisa y no se guarda nada.
    void administratorDemotedMeanwhileIsDenied()
    {
        QList<SystemParameterEditDto> edits = untouched();
        edit(edits, SystemParameter::BankBalance).edited = 6000000;
        m_settings.activeAdministrators.clear();
        QCOMPARE(service().updateSystemParameters(edits, admin()).status, Status::Denied);
        QCOMPARE(m_settings.actors, QList<int>{fakes::InMemorySettingsRepository::kAdministratorId});
        QCOMPARE(m_settings.rows.value(SystemParameter::BankBalance).hundredths,
                 std::optional<qint64>(5000000));
    }

    void failedReadIsReported()
    {
        m_settings.failure = QStringLiteral("sin conexión");
        const auto result = service().getSystemParameters(admin());
        QCOMPARE(result.status, application::SystemParametersResult::Status::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("sin conexión"));
    }

    // Sin cambios no se toca la base, ni por los que nunca se capturaron.
    void unchangedFormDoesNotTouchTheDatabase()
    {
        const auto result = service().updateSystemParameters(untouched(), admin());
        QCOMPARE(result.status, Status::NoChanges);
        QCOMPARE(m_settings.updates.size(), 0);
    }

    // Solo lo que cambió viaja, con el valor que tenía al leerse.
    void onlyChangedParametersAreSaved()
    {
        QList<SystemParameterEditDto> edits = untouched();
        edit(edits, SystemParameter::UmaDailyValue).edited = 12000;
        edit(edits, SystemParameter::SafeBalance).edited = 1000000;

        const auto result = service().updateSystemParameters(edits, admin());
        QCOMPARE(result.status, Status::Saved);
        QCOMPARE(result.updated, (QList<SystemParameter>{SystemParameter::UmaDailyValue,
                                                         SystemParameter::SafeBalance}));
        QCOMPARE(m_settings.updates.size(), 1);
        const auto &changes = m_settings.updates.first();
        QCOMPARE(changes.size(), 2);
        QCOMPARE(changes.at(0).expected, std::optional<qint64>(11731));
        QCOMPARE(changes.at(0).value, qint64(12000));
        // Caja Fuerte no tenía renglón: se espera que siga sin tenerlo.
        QCOMPARE(changes.at(1).expected, std::nullopt);
    }

    // Se valida solo lo que cambió, y nada se guarda si algo no pasa.
    void invalidValuesAreRejectedWithoutSaving()
    {
        QList<SystemParameterEditDto> edits = untouched();
        edit(edits, SystemParameter::CashLimitUmaMultiple).edited = 500000; // 5,000 UMA
        edit(edits, SystemParameter::PettyCashBalance).edited = -100;
        edit(edits, SystemParameter::BankBalance).edited = 6000000;

        const auto result = service().updateSystemParameters(edits, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(),
                 (QStringList{QStringLiteral("cashLimitUmaMultiple"), QStringLiteral("pettyCashBalance")}));
        QCOMPARE(m_settings.updates.size(), 0);
    }

    // Una UMA que ya estaba no se puede dejar vacía.
    void storedUmaCannotBeCleared()
    {
        QList<SystemParameterEditDto> edits = untouched();
        edit(edits, SystemParameter::UmaDailyValue).edited.reset();
        const auto result = service().updateSystemParameters(edits, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("umaDailyValue")});
    }

    // Otro administrador la cambió desde que se leyó: no se pisa.
    void staleValueIsAConflict()
    {
        QList<SystemParameterEditDto> edits = untouched();
        m_settings.store(SystemParameter::UmaDailyValue, 11800);
        edit(edits, SystemParameter::UmaDailyValue).edited = 12000;

        const auto result = service().updateSystemParameters(edits, admin());
        QCOMPARE(result.status, Status::Conflict);
        QCOMPARE(m_settings.rows.value(SystemParameter::UmaDailyValue).hundredths,
                 std::optional<qint64>(11800));
    }

    void failedSaveIsReported()
    {
        QList<SystemParameterEditDto> edits = untouched();
        edit(edits, SystemParameter::BankBalance).edited = 1;
        m_settings.failure = QStringLiteral("se cayó la base");
        const auto result = service().updateSystemParameters(edits, admin());
        QCOMPARE(result.status, Status::Failed);
        QCOMPARE(result.errorMessage, QStringLiteral("se cayó la base"));
    }
};

QTEST_APPLESS_MAIN(TstSystemSettingsService)
#include "tst_systemsettingsservice.moc"
