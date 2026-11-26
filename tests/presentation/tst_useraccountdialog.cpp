// El formulario de una cuenta de usuario (US-07.1) con sus widgets reales: qué
// campos muestra cada modo, qué devuelve, y que en la propia cuenta el rol
// queda bloqueado.

#include "presentation/settings/useraccountdialog.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QPushButton>
#include <QtTest>

using Mode = UserAccountDialog::Mode;

class TstUserAccountDialog : public QObject
{
    Q_OBJECT

private:
    static application::UserAccountDto seller()
    {
        application::UserAccountDto user;
        user.id = 7;
        user.username = QStringLiteral("vendedor");
        user.displayName = QStringLiteral("Juan Pérez");
        user.role = domain::UserRole::SalesAgent;
        user.version = QStringLiteral("123");
        user.canChangeRole = true;
        user.canChangeActive = true;
        return user;
    }

    static QLineEdit *edit(UserAccountDialog &dialog, const char *name)
    {
        return dialog.findChild<QLineEdit *>(QString::fromLatin1(name));
    }

private slots:
    void createAsksForEverything()
    {
        UserAccountDialog dialog(Mode::Create, {});
        dialog.show();
        QVERIFY(edit(dialog, "usernameEdit")->isEnabled());
        QVERIFY(edit(dialog, "passwordEdit")->isVisible());
        QCOMPARE(edit(dialog, "passwordEdit")->echoMode(), QLineEdit::Password);

        edit(dialog, "usernameEdit")->setText(QStringLiteral("ana"));
        edit(dialog, "displayNameEdit")->setText(QStringLiteral("Ana"));
        edit(dialog, "passwordEdit")->setText(QStringLiteral("clave segura"));
        edit(dialog, "passwordConfirmationEdit")->setText(QStringLiteral("clave segura"));
        auto *role = dialog.findChild<QComboBox *>(QStringLiteral("roleCombo"));
        role->setCurrentIndex(role->findText(QStringLiteral("Administrador")));
        const application::NewUserAccountDto user = dialog.newUser();
        QCOMPARE(user.username, QStringLiteral("ana"));
        QCOMPARE(user.passwordConfirmation, QStringLiteral("clave segura"));
        QCOMPARE(user.role, domain::UserRole::Admin);
    }

    // Editar: el usuario no cambia y no hay contraseña; lleva la versión leída.
    void editShowsNameAndRole()
    {
        UserAccountDialog dialog(Mode::Edit, seller());
        dialog.show();
        QVERIFY(!edit(dialog, "usernameEdit")->isEnabled());
        QCOMPARE(edit(dialog, "usernameEdit")->text(), QStringLiteral("vendedor"));
        QVERIFY(!edit(dialog, "passwordEdit")->isVisible());
        edit(dialog, "displayNameEdit")->setText(QStringLiteral("Juan"));
        const application::UserAccountUpdateDto update = dialog.userUpdate();
        QCOMPARE(update.id, 7);
        QCOMPARE(update.version, QStringLiteral("123"));
        QCOMPARE(update.displayName, QStringLiteral("Juan"));
        QCOMPARE(update.role, domain::UserRole::SalesAgent);
    }

    void ownRoleIsLocked()
    {
        application::UserAccountDto self = seller();
        self.canChangeRole = false;
        UserAccountDialog dialog(Mode::Edit, self);
        QVERIFY(!dialog.findChild<QComboBox *>(QStringLiteral("roleCombo"))->isEnabled());
    }

    void passwordModeAsksOnlyForThePassword()
    {
        UserAccountDialog dialog(Mode::Password, seller());
        dialog.show();
        QVERIFY(!edit(dialog, "displayNameEdit")->isVisible());
        QVERIFY(!dialog.findChild<QComboBox *>(QStringLiteral("roleCombo"))->isVisible());
        edit(dialog, "passwordEdit")->setText(QStringLiteral("nueva clave"));
        edit(dialog, "passwordConfirmationEdit")->setText(QStringLiteral("nueva clave"));
        QCOMPARE(dialog.passwordReset().id, 7);
        QCOMPARE(dialog.passwordReset().password, QStringLiteral("nueva clave"));
    }

    // Aceptar avisa y no cierra: lo cierra quien guarda.
    void acceptOnlyAnnounces()
    {
        UserAccountDialog dialog(Mode::Create, {});
        dialog.show();
        QSignalSpy submitted(&dialog, &UserAccountDialog::submitted);
        dialog.findChild<QPushButton *>(QStringLiteral("acceptButton"))->click();
        QCOMPARE(submitted.size(), 1);
        QVERIFY(dialog.isVisible());
    }

    void errorsAppearUnderTheirFields()
    {
        UserAccountDialog dialog(Mode::Create, {});
        dialog.show();
        dialog.showErrors({{QStringLiteral("username"), QStringLiteral("Ya hay una cuenta con ese usuario.")}});
        QCOMPARE(edit(dialog, "usernameEdit")->property("hasError").toBool(), true);
    }

    // Un mensaje largo se lee completo: el diálogo crece lo que haga falta.
    void longErrorIsShownWhole()
    {
        UserAccountDialog dialog(Mode::Create, {});
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.showErrors({{QStringLiteral("username"),
                            QStringLiteral("El usuario lleva de 3 a 50 caracteres: letras sin acento, "
                                           "números, punto, guion o guion bajo, y empieza con letra o "
                                           "número.")}});
        QCoreApplication::processEvents();
        auto *label = dialog.findChild<QLabel *>(QStringLiteral("usernameEditError"));
        QVERIFY(label && label->isVisible());
        QVERIFY2(label->height() >= label->heightForWidth(label->width()),
                 qPrintable(QStringLiteral("%1 < %2").arg(label->height())
                                .arg(label->heightForWidth(label->width()))));
    }

    // Mientras guarda no se cierra (ni Cancelar ni Esc): el resultado tiene
    // dónde mostrarse.
    void cannotBeClosedWhileSaving()
    {
        UserAccountDialog dialog(Mode::Create, {});
        dialog.show();
        dialog.setBusy(true);
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("cancelButton"));
        QVERIFY(!cancel->isEnabled());
        dialog.reject();
        QVERIFY(dialog.isVisible());
        QVERIFY(!dialog.close());
        QVERIFY(dialog.isVisible());
        dialog.setBusy(false);
        dialog.reject();
        QVERIFY(!dialog.isVisible());
    }
};

QTEST_MAIN(TstUserAccountDialog)
#include "tst_useraccountdialog.moc"
