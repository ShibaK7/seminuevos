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

QStringList errorsFor(SystemParameter parameter, qint64 hundredths,
                      std::optional<qint64> current = std::nullopt)
{
    domain::ValidationResult result;
    SystemParameterPolicy::check(parameter, hundredths, current, result);
    return result.fields();
}

} // namespace

class TstSystemParameters : public QObject
{
    Q_OBJECT

private slots:
    // La UMA y el tope, en su orden, cada uno con su propio campo. Los saldos
    // ya no son parámetros: son cuentas (domain/finance).
    void parametersInScreenOrder()
    {
        const QList<SystemParameter> all = domain::allSystemParameters();
        QCOMPARE(all.size(), 2);
        QCOMPARE(domain::displayLabel(all.first()), QStringLiteral("Valor 1 UMA"));
        QCOMPARE(domain::displayLabel(all.last()), QStringLiteral("Máximo valor pagos en efectivo"));
        QVERIFY(domain::fieldName(all.first()) != domain::fieldName(all.last()));
        QCOMPARE(domain::unitOf(SystemParameter::CashLimitUmaMultiple), domain::ParameterUnit::Uma);
        QCOMPARE(domain::unitOf(SystemParameter::UmaDailyValue), domain::ParameterUnit::Pesos);
    }

    // La UMA no tiene valor por omisión (cambia cada año); el tope es el legal.
    void defaults()
    {
        QCOMPARE(domain::defaultHundredths(SystemParameter::UmaDailyValue), std::nullopt);
        QCOMPARE(domain::defaultHundredths(SystemParameter::CashLimitUmaMultiple),
                 std::optional<qint64>(321000));
    }

    void umaMustBePositive()
    {
        QVERIFY(errorsFor(SystemParameter::UmaDailyValue, 11731).isEmpty());
        QCOMPARE(errorsFor(SystemParameter::UmaDailyValue, 0), QStringList{QStringLiteral("umaDailyValue")});
    }

    // Un punto decimal fuera de lugar multiplicaría (o dividiría) el tope de
    // efectivo: la UMA nueva queda entre la mitad y el doble de la vigente.
    void umaCannotJumpFromTheCurrentOne_data()
    {
        QTest::addColumn<qint64>("current");
        QTest::addColumn<qint64>("hundredths");
        QTest::addColumn<bool>("valid");
        QTest::newRow("el aumento de un año") << qint64(11731) << qint64(12200) << true;
        QTest::newRow("justo el doble") << qint64(11731) << qint64(23462) << true;
        QTest::newRow("más del doble") << qint64(11731) << qint64(23463) << false;
        QTest::newRow("punto olvidado") << qint64(11731) << qint64(1173100) << false;
        QTest::newRow("justo la mitad") << qint64(11730) << qint64(5865) << true;
        QTest::newRow("menos de la mitad") << qint64(11731) << qint64(5865) << false;
        QTest::newRow("punto corrido") << qint64(11731) << qint64(1173) << false;
        // Con la UMA en miles de pesos (dentro de un siglo) sigue sirviendo.
        QTest::newRow("dentro de un siglo") << qint64(1500000) << qint64(1560000) << true;
    }

    void umaCannotJumpFromTheCurrentOne()
    {
        QFETCH(qint64, current);
        QFETCH(qint64, hundredths);
        QFETCH(bool, valid);
        QCOMPARE(errorsFor(SystemParameter::UmaDailyValue, hundredths, current).isEmpty(), valid);
    }

    void umaJumpErrorNamesTheCurrentValue()
    {
        domain::ValidationResult result;
        SystemParameterPolicy::check(SystemParameter::UmaDailyValue, 1173100, 11731, result);
        QVERIFY(result.firstMessage().contains(QStringLiteral("$117.31")));
    }

    // Sin una vigente no hay contra qué comparar: basta con que sea positiva.
    void firstUmaOnlyNeedsToBePositive()
    {
        QVERIFY(errorsFor(SystemParameter::UmaDailyValue, 1173100).isEmpty());
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

    // Lo que no cabe en NUMERIC(12, 2) se rechaza antes que cualquier otra
    // regla.
    void valuesFitTheColumn()
    {
        QCOMPARE(errorsFor(SystemParameter::CashLimitUmaMultiple, SystemParameterPolicy::kMaxHundredths + 1),
                 QStringList{QStringLiteral("cashLimitUmaMultiple")});
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
