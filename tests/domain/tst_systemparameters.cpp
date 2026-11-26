// Los parámetros de Configuraciones (US-07.1): qué valores acepta cada uno,
// cuáles trae por omisión y quién puede cambiarlos. Fijan las decisiones del
// negocio: el tope de efectivo se puede bajar pero nunca pasar del legal, y
// solo el Administrador entra.

#include "domain/settings/rules/settingsaccess.h"
#include "domain/settings/rules/systemparameterpolicy.h"
#include "domain/settings/value_objects/systemparameter.h"

#include <QSet>
#include <QtTest>

using domain::SystemParameter;
using domain::SystemParameterPolicy;

namespace {

QStringList errorsFor(SystemParameter parameter, qint64 hundredths)
{
    domain::ValidationResult result;
    SystemParameterPolicy::check(parameter, hundredths, result);
    return result.fields();
}

} // namespace

class TstSystemParameters : public QObject
{
    Q_OBJECT

private slots:
    // Los cinco del mockup, en su orden, cada uno con su propio campo.
    void fiveParametersInScreenOrder()
    {
        const QList<SystemParameter> all = domain::allSystemParameters();
        QCOMPARE(all.size(), 5);
        QCOMPARE(domain::displayLabel(all.first()), QStringLiteral("Valor 1 UMA"));
        QCOMPARE(domain::displayLabel(all.last()), QStringLiteral("Caja Fuerte"));
        QSet<QString> fields;
        for (const SystemParameter parameter : all)
            fields.insert(domain::fieldName(parameter));
        QCOMPARE(fields.size(), all.size());
        QCOMPARE(domain::unitOf(SystemParameter::CashLimitUmaMultiple), domain::ParameterUnit::Uma);
        QCOMPARE(domain::unitOf(SystemParameter::BankBalance), domain::ParameterUnit::Pesos);
    }

    // La UMA no tiene valor por omisión (cambia cada año); el tope es el legal
    // y los saldos empiezan en cero.
    void defaults()
    {
        QCOMPARE(domain::defaultHundredths(SystemParameter::UmaDailyValue), std::nullopt);
        QCOMPARE(domain::defaultHundredths(SystemParameter::CashLimitUmaMultiple),
                 std::optional<qint64>(321000));
        QCOMPARE(domain::defaultHundredths(SystemParameter::SafeBalance), std::optional<qint64>(0));
    }

    void umaMustBePositive()
    {
        QVERIFY(errorsFor(SystemParameter::UmaDailyValue, 11731).isEmpty());
        QCOMPARE(errorsFor(SystemParameter::UmaDailyValue, 0), QStringList{QStringLiteral("umaDailyValue")});
    }

    // Un punto decimal fuera de lugar multiplicaría el tope de efectivo.
    void umaHasAPlausibleCeiling()
    {
        QVERIFY(errorsFor(SystemParameter::UmaDailyValue, 100000).isEmpty());
        QCOMPARE(errorsFor(SystemParameter::UmaDailyValue, 100001),
                 QStringList{QStringLiteral("umaDailyValue")});
        QCOMPARE(errorsFor(SystemParameter::UmaDailyValue, 1173100),
                 QStringList{QStringLiteral("umaDailyValue")});
    }

    // Más estricto que la ley sí; mayor, fraccionario o cero, no.
    void cashLimitIsAWholeNumberUpToTheLegalOne_data()
    {
        QTest::addColumn<qint64>("hundredths");
        QTest::addColumn<bool>("valid");
        QTest::newRow("el legal") << qint64(321000) << true;
        QTest::newRow("más estricto") << qint64(100000) << true;
        QTest::newRow("una UMA") << qint64(100) << true;
        QTest::newRow("arriba de la ley") << qint64(321100) << false;
        QTest::newRow("fraccionario") << qint64(100050) << false;
        QTest::newRow("cero") << qint64(0) << false;
    }

    void cashLimitIsAWholeNumberUpToTheLegalOne()
    {
        QFETCH(qint64, hundredths);
        QFETCH(bool, valid);
        QCOMPARE(errorsFor(SystemParameter::CashLimitUmaMultiple, hundredths).isEmpty(), valid);
    }

    void balancesAreNotNegative()
    {
        QVERIFY(errorsFor(SystemParameter::PettyCashBalance, 0).isEmpty());
        QVERIFY(errorsFor(SystemParameter::BankBalance, 5000000).isEmpty());
        QCOMPARE(errorsFor(SystemParameter::SafeBalance, -1), QStringList{QStringLiteral("safeBalance")});
    }

    // Lo que cabe en NUMERIC(12, 2), y ni un centavo más.
    void valuesFitTheColumn()
    {
        QVERIFY(errorsFor(SystemParameter::BankBalance, SystemParameterPolicy::kMaxHundredths).isEmpty());
        QCOMPARE(errorsFor(SystemParameter::BankBalance, SystemParameterPolicy::kMaxHundredths + 1),
                 QStringList{QStringLiteral("bankBalance")});
    }

    void onlyTheAdministratorEntersSettings()
    {
        const domain::SettingsAccess admin(domain::UserRole::Admin);
        QVERIFY(admin.canManageSystemParameters());
        QVERIFY(admin.canManageUsers());
        QVERIFY(admin.canOpenSettings());
        const domain::SettingsAccess seller(domain::UserRole::SalesAgent);
        QVERIFY(!seller.canManageSystemParameters());
        QVERIFY(!seller.canManageUsers());
        QVERIFY(!seller.canOpenSettings());
    }
};

QTEST_APPLESS_MAIN(TstSystemParameters)
#include "tst_systemparameters.moc"
