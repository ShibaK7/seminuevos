// Consultar y mover las cuentas de dinero desde Configuraciones: solo el
// Administrador, los saldos iniciales una sola vez, y nada saca más de lo que
// hay (tampoco si otra sesión lo gastó entre leer y guardar).

#include "application/finance/services/cashledgerservice.h"
#include "cashfakes.h"

#include <QtTest>

using domain::CashAccount;
using Status = application::CashWriteResult::Status;

namespace {

const QDate kToday(2026, 11, 26);

application::SessionDto admin()
{
    return {QStringLiteral("admin"), QStringLiteral("Admin"), domain::UserRole::Admin,
            fakes::InMemoryCashLedger::kAdministratorId};
}

application::SessionDto seller()
{
    return {QStringLiteral("vendedor"), QStringLiteral("Vendedor"), domain::UserRole::SalesAgent, 2};
}

} // namespace

class TstCashLedgerService : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryCashLedger m_ledger;

    application::CashLedgerService service() { return application::CashLedgerService(m_ledger); }

private slots:
    void init() { m_ledger = fakes::InMemoryCashLedger(); }

    // Las tres, en el orden de pantalla, con los movimientos más recientes.
    void overviewListsTheThreeInOrder()
    {
        m_ledger.setBalance(CashAccount::Bank, 100);
        const auto result = service().overview(admin());
        QCOMPARE(result.status, application::CashOverviewResult::Status::Loaded);
        QCOMPARE(result.balances.size(), 3);
        QCOMPARE(result.balances.at(0).account, CashAccount::PettyCash);
        QCOMPARE(result.balances.at(1).hundredths, qint64(100));
        QCOMPARE(result.balances.at(2).account, CashAccount::Safe);
        QVERIFY(result.balances.at(2).opened);
    }

    void sellerIsDeniedWithoutTouchingTheLedger()
    {
        QCOMPARE(service().overview(seller()).status, application::CashOverviewResult::Status::Denied);
        QCOMPARE(service().openBalances({100, 100}, seller()).status, Status::Denied);
        QCOMPARE(service().depositToSafe({CashAccount::PettyCash, 1}, seller()).status, Status::Denied);
        QCOMPARE(service().withdrawFromSafe({kToday, QStringLiteral("x"), 1}, seller(), kToday).status,
                 Status::Denied);
        QCOMPARE(m_ledger.writes, 0);
    }

    // Una sola vez: la segunda es un conflicto y no suma nada.
    void openingHappensOnce()
    {
        QCOMPARE(service().openBalances({2000000, 10000000}, admin()).status, Status::Saved);
        QCOMPARE(m_ledger.balance(CashAccount::PettyCash), qint64(2000000));
        QCOMPARE(m_ledger.balance(CashAccount::Bank), qint64(10000000));
        QCOMPARE(m_ledger.movements.size(), 2);

        QCOMPARE(service().openBalances({1, 1}, admin()).status, Status::Conflict);
        QCOMPARE(m_ledger.balance(CashAccount::PettyCash), qint64(2000000));
    }

    // Lo que ya entró antes de abrir está en lo contado: no se suma dos veces.
    void openingSetsTheCountedBalance()
    {
        m_ledger.accounts[CashAccount::PettyCash].hundredths = 5000000; // una venta, sin abrir
        QCOMPARE(service().openBalances({2000000, 0}, admin()).status, Status::Saved);
        QCOMPARE(m_ledger.balance(CashAccount::PettyCash), qint64(2000000));
        QCOMPARE(m_ledger.movements.last().hundredths, qint64(-3000000));
    }

    void negativeOpeningIsRejected()
    {
        const auto result = service().openBalances({-1, 0}, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("pettyCashOpening")});
        QCOMPARE(m_ledger.writes, 0);
    }

    // El traspaso resta del origen y suma a Caja Fuerte.
    void depositMovesMoneyIntoTheSafe()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 500000);
        QCOMPARE(service().depositToSafe({CashAccount::PettyCash, 200000}, admin()).status, Status::Saved);
        QCOMPARE(m_ledger.balance(CashAccount::PettyCash), qint64(300000));
        QCOMPARE(m_ledger.balance(CashAccount::Safe), qint64(200000));
    }

    // No más de lo que hay en el origen, sin tocar la bitácora.
    void depositIsLimitedByTheSourceBalance()
    {
        m_ledger.setBalance(CashAccount::Bank, 100000);
        const auto result = service().depositToSafe({CashAccount::Bank, 100001}, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.firstMessage(),
                 QStringLiteral("Cantidad Banco solo tiene $1,000.00 disponibles."));
        QCOMPARE(m_ledger.writes, 0);
    }

    // Otra sesión gastó el dinero entre leer y guardar: rechazo con el saldo
    // vigente, nada aplicado.
    void moneySpentMeanwhileIsRejectedWithTheCurrentBalance()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        m_ledger.spentMeanwhile[CashAccount::PettyCash] = 60000;
        const auto result = service().depositToSafe({CashAccount::PettyCash, 50000}, admin());
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(), QStringList{QStringLiteral("amount")});
        QVERIFY(result.validation.firstMessage().contains(QStringLiteral("$400.00")));
        QCOMPARE(m_ledger.balance(CashAccount::Safe), qint64(0));
    }

    void withdrawalTakesFromTheSafe()
    {
        m_ledger.setBalance(CashAccount::Safe, 300000);
        const auto result =
            service().withdrawFromSafe({kToday, QStringLiteral("  Pago de nómina  "), 100000}, admin(), kToday);
        QCOMPARE(result.status, Status::Saved);
        QCOMPARE(m_ledger.balance(CashAccount::Safe), qint64(200000));
        QCOMPARE(m_ledger.movements.last().concept, QStringLiteral("Pago de nómina"));
        QCOMPARE(m_ledger.movements.last().date, kToday);
    }

    void withdrawalNeedsWhenWhatAndHowMuch()
    {
        m_ledger.setBalance(CashAccount::Safe, 100);
        const auto result = service().withdrawFromSafe({kToday.addDays(1), QString(), 101}, admin(), kToday);
        QCOMPARE(result.status, Status::Rejected);
        QCOMPARE(result.validation.fields(),
                 (QStringList{QStringLiteral("date"), QStringLiteral("concept"), QStringLiteral("amount")}));
        QCOMPARE(m_ledger.writes, 0);
    }

    // La sesión dice Administrador, pero en la base ya no lo es.
    void demotedAdministratorIsDenied()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100);
        m_ledger.activeAdministrators.clear();
        QCOMPARE(service().depositToSafe({CashAccount::PettyCash, 100}, admin()).status, Status::Denied);
        QCOMPARE(m_ledger.balance(CashAccount::PettyCash), qint64(100));
    }

    void failuresAreReported()
    {
        m_ledger.failure = QStringLiteral("se cayó la base");
        const auto overview = service().overview(admin());
        QCOMPARE(overview.status, application::CashOverviewResult::Status::Failed);
        QCOMPARE(overview.errorMessage, QStringLiteral("se cayó la base"));
        const auto deposit = service().depositToSafe({CashAccount::PettyCash, 1}, admin());
        QCOMPARE(deposit.status, Status::Failed);
    }
};

QTEST_APPLESS_MAIN(TstCashLedgerService)
#include "tst_cashledgerservice.moc"
