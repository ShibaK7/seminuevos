// Las reglas de las cuentas de usuario de Configuraciones (US-07.1): el
// formato del usuario, el nombre, la contraseña, y que nadie se cambie su
// propio rol ni se desactive.

#include "domain/settings/rules/useraccountpolicy.h"

#include <QtTest>

using domain::UserAccountPolicy;
using domain::UserRole;

class TstUserAccountPolicy : public QObject
{
    Q_OBJECT

private slots:
    // Se guarda en minúsculas y sin espacios alrededor.
    void usernameIsNormalized()
    {
        QCOMPARE(UserAccountPolicy::normalizeUsername(QStringLiteral("  Ana.Lopez ")),
                 QStringLiteral("ana.lopez"));
    }

    void usernameFormat_data()
    {
        QTest::addColumn<QString>("username");
        QTest::addColumn<bool>("valid");
        QTest::newRow("simple") << QStringLiteral("ana") << true;
        QTest::newRow("con punto, guion y número") << QStringLiteral("ana.lopez-2_b") << true;
        QTest::newRow("vacío") << QString() << false;
        QTest::newRow("dos letras") << QStringLiteral("an") << false;
        QTest::newRow("empieza con punto") << QStringLiteral(".ana") << false;
        QTest::newRow("con espacio") << QStringLiteral("ana lopez") << false;
        QTest::newRow("con acento") << QStringLiteral("josé") << false;
        QTest::newRow("51 caracteres") << QString(51, QLatin1Char('a')) << false;
        QTest::newRow("50 caracteres") << QString(50, QLatin1Char('a')) << true;
    }

    void usernameFormat()
    {
        QFETCH(QString, username);
        QFETCH(bool, valid);
        domain::ValidationResult result;
        UserAccountPolicy::checkUsername(username, result);
        QCOMPARE(result.isValid(), valid);
    }

    void displayNameIsRequired()
    {
        domain::ValidationResult result;
        UserAccountPolicy::checkDisplayName(QStringLiteral("   "), result);
        QCOMPARE(result.fields(), QStringList{QStringLiteral("displayName")});
    }

    void passwordLengthAndConfirmation()
    {
        domain::ValidationResult shortOne;
        UserAccountPolicy::checkPassword(QStringLiteral("1234567"), QStringLiteral("1234567"), shortOne);
        QCOMPARE(shortOne.fields(), QStringList{QStringLiteral("password")});

        domain::ValidationResult mismatch;
        UserAccountPolicy::checkPassword(QStringLiteral("12345678"), QStringLiteral("12345679"), mismatch);
        QCOMPARE(mismatch.fields(), QStringList{QStringLiteral("passwordConfirmation")});

        domain::ValidationResult good;
        UserAccountPolicy::checkPassword(QStringLiteral("clave segura"), QStringLiteral("clave segura"), good);
        QVERIFY(good.isValid());
    }

    // A otra cuenta sí; a la propia, ni el rol ni la desactivación.
    void nobodyChangesTheirOwnRoleOrActivation()
    {
        QVERIFY(UserAccountPolicy::canChangeRole(1, 2));
        QVERIFY(!UserAccountPolicy::canChangeRole(1, 1));
        QVERIFY(!UserAccountPolicy::canChangeActive(1, 1));

        domain::ValidationResult ownRole;
        UserAccountPolicy::checkRoleChange(1, 1, UserRole::Admin, UserRole::SalesAgent, ownRole);
        QCOMPARE(ownRole.fields(), QStringList{QStringLiteral("role")});

        // Guardar la propia cuenta sin tocar el rol sí se puede.
        domain::ValidationResult sameRole;
        UserAccountPolicy::checkRoleChange(1, 1, UserRole::Admin, UserRole::Admin, sameRole);
        QVERIFY(sameRole.isValid());

        domain::ValidationResult ownActivation;
        UserAccountPolicy::checkActivation(1, 1, ownActivation);
        QCOMPARE(ownActivation.fields(), QStringList{QStringLiteral("active")});
    }
};

QTEST_APPLESS_MAIN(TstUserAccountPolicy)
#include "tst_useraccountpolicy.moc"
