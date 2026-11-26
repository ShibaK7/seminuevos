// El formulario de un movimiento a mano en las cuentas de dinero, con sus
// widgets reales: qué campos muestra cada modo, qué devuelve, y que el monto
// no puede pasar de lo que hay en la cuenta de la que sale.

#include "presentation/settings/cashmovementdialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

using domain::CashAccount;
using Mode = CashMovementDialog::Mode;

namespace {

QList<application::CashBalanceDto> balances()
{
    QList<application::CashBalanceDto> list;
    for (const auto &[account, hundredths] :
         QList<std::pair<CashAccount, qint64>>{{CashAccount::PettyCash, 150000},
                                               {CashAccount::Bank, 20000},
                                               {CashAccount::Safe, 30000}}) {
        application::CashBalanceDto balance;
        balance.account = account;
        balance.hundredths = hundredths;
        balance.opened = true;
        list << balance;
    }
    return list;
}

} // namespace

class TstCashMovementDialog : public QObject
{
    Q_OBJECT

private:
    template <class T>
    static T *find(CashMovementDialog &dialog, const char *name)
    {
        return dialog.findChild<T *>(QString::fromLatin1(name));
    }

private slots:
    // Los saldos iniciales: Caja Chica y Cantidad Banco, nada más.
    void openingAsksForTheTwoAccounts()
    {
        CashMovementDialog dialog(Mode::Opening, {});
        dialog.show();
        QVERIFY(find<QDoubleSpinBox>(dialog, "pettyCashOpeningSpin")->isVisible());
        QVERIFY(find<QDoubleSpinBox>(dialog, "bankOpeningSpin")->isVisible());
        QVERIFY(!find<QDoubleSpinBox>(dialog, "amountSpin")->isVisible());
        QVERIFY(!find<QComboBox>(dialog, "sourceCombo")->isVisible());
        find<QDoubleSpinBox>(dialog, "pettyCashOpeningSpin")->setValue(20000.0);
        find<QDoubleSpinBox>(dialog, "bankOpeningSpin")->setValue(100000.55);
        const application::OpeningBalancesDto opening = dialog.opening();
        QCOMPARE(opening.pettyCashHundredths, qint64(2000000));
        QCOMPARE(opening.bankHundredths, qint64(10000055));
    }

    // El monto no pasa de lo que hay en el origen, y dice cuánto hay.
    void depositIsLimitedByTheChosenSource()
    {
        CashMovementDialog dialog(Mode::Deposit, balances());
        dialog.show();
        auto *source = find<QComboBox>(dialog, "sourceCombo");
        auto *amount = find<QDoubleSpinBox>(dialog, "amountSpin");
        QCOMPARE(source->count(), 2);
        QCOMPARE(source->currentText(), QStringLiteral("Caja Chica"));
        QCOMPARE(amount->maximum(), 1500.0);
        source->setCurrentIndex(1);
        QCOMPARE(amount->maximum(), 200.0);
        QCOMPARE(find<QLabel>(dialog, "availableLabel")->text(),
                 QStringLiteral("Disponible en Cantidad Banco: $200.00"));
        amount->setValue(5000.0); // se queda en el máximo
        const application::SafeDepositDto deposit = dialog.deposit();
        QCOMPARE(deposit.source, CashAccount::Bank);
        QCOMPARE(deposit.hundredths, qint64(20000));
        QVERIFY(!find<QLineEdit>(dialog, "conceptEdit")->isVisible());
    }

    // El retiro: cuándo (no después de hoy), concepto y cuánto (no más de lo
    // que hay en Caja Fuerte).
    void withdrawalAsksWhenWhatAndHowMuch()
    {
        CashMovementDialog dialog(Mode::Withdrawal, balances());
        dialog.show();
        QVERIFY(!find<QComboBox>(dialog, "sourceCombo")->isVisible());
        auto *date = find<QDateEdit>(dialog, "dateEdit");
        QCOMPARE(date->maximumDate(), QDate::currentDate());
        QCOMPARE(find<QDoubleSpinBox>(dialog, "amountSpin")->maximum(), 300.0);
        date->setDate(QDate::currentDate().addDays(-2));
        find<QLineEdit>(dialog, "conceptEdit")->setText(QStringLiteral("Pago de nómina"));
        find<QDoubleSpinBox>(dialog, "amountSpin")->setValue(120.5);
        const application::SafeWithdrawalDto withdrawal = dialog.withdrawal();
        QCOMPARE(withdrawal.date, QDate::currentDate().addDays(-2));
        QCOMPARE(withdrawal.concept, QStringLiteral("Pago de nómina"));
        QCOMPARE(withdrawal.hundredths, qint64(12050));
    }

    void acceptOnlyAnnouncesAndErrorsAppearUnderTheirFields()
    {
        CashMovementDialog dialog(Mode::Withdrawal, balances());
        dialog.show();
        QSignalSpy submitted(&dialog, &CashMovementDialog::submitted);
        dialog.findChild<QPushButton *>(QStringLiteral("acceptButton"))->click();
        QCOMPARE(submitted.size(), 1);
        QVERIFY(dialog.isVisible());
        dialog.showErrors({{QStringLiteral("concept"), QStringLiteral("Captura el concepto del retiro.")}});
        QCOMPARE(find<QLineEdit>(dialog, "conceptEdit")->property("hasError").toBool(), true);
    }

    void cannotBeClosedWhileSaving()
    {
        CashMovementDialog dialog(Mode::Deposit, balances());
        dialog.show();
        dialog.setBusy(true);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("cancelButton"))->isEnabled());
        dialog.reject();
        QVERIFY(dialog.isVisible());
        dialog.setBusy(false);
        dialog.reject();
        QVERIFY(!dialog.isVisible());
    }
};

QTEST_MAIN(TstCashMovementDialog)
#include "tst_cashmovementdialog.moc"
