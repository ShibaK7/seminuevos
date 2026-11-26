// El presenter de la tarjeta de saldos de Configuraciones con una vista falsa
// y el servicio real sobre las cuentas en memoria: los saldos, los saldos
// iniciales una sola vez, el ingreso y el retiro de Caja Fuerte, y lo que pasa
// si falla, choca o no hay saldo.

#include "application/finance/services/cashledgerservice.h"
#include "cashfakes.h"
#include "fakes.h"
#include "presentation/settings/cashbalancespresenter.h"
#include "presentation/settings/icashbalancesview.h"

#include <QtTest>

using domain::CashAccount;

namespace {

class FakeCashBalancesView final : public presentation::ICashBalancesView
{
public:
    bool busy = false;
    QList<application::CashBalanceDto> balances;
    QList<application::CashMovementDto> movements;
    std::optional<CashAccount> filter;
    // El formulario abierto: "", "apertura", "ingreso" o "retiro".
    QString form;
    QList<application::CashBalanceDto> depositBalances;
    qint64 withdrawalAvailable = -1;
    bool confirm = true;
    int confirmations = 0;
    bool formBusy = false;
    QList<domain::ValidationError> formErrors;
    QString message;
    QString notice;

    void setBusy(bool value) override { busy = value; }
    void showBalances(const QList<application::CashBalanceDto> &value) override { balances = value; }
    void showMovements(const QList<application::CashMovementDto> &value) override { movements = value; }
    std::optional<CashAccount> movementFilter() const override { return filter; }
    void askOpening() override { form = QStringLiteral("apertura"); }
    void askSafeDeposit(const QList<application::CashBalanceDto> &value) override
    {
        form = QStringLiteral("ingreso");
        depositBalances = value;
    }
    void askSafeWithdrawal(qint64 available) override
    {
        form = QStringLiteral("retiro");
        withdrawalAvailable = available;
    }
    bool confirmOpening(const application::OpeningBalancesDto &) override
    {
        ++confirmations;
        return confirm;
    }
    void setFormBusy(bool value) override { formBusy = value; }
    void showFormErrors(const QList<domain::ValidationError> &value) override { formErrors = value; }
    void closeForm() override { form.clear(); }
    void showMessage(const QString &value) override { message = value; }
    void showNotice(const QString &value) override { notice = value; }

    qint64 balance(CashAccount account) const
    {
        for (const application::CashBalanceDto &dto : balances) {
            if (dto.account == account)
                return dto.hundredths;
        }
        return -1;
    }
};

} // namespace

class TstCashBalancesPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryCashLedger m_ledger;
    std::unique_ptr<application::CashLedgerService> m_service;
    FakeCashBalancesView m_view;
    fakes::InlineTaskRunner m_runner;

    std::unique_ptr<presentation::CashBalancesPresenter> make(presentation::TaskRunner &runner,
                                                             domain::UserRole role = domain::UserRole::Admin)
    {
        const application::SessionDto actor{QStringLiteral("admin"), QStringLiteral("Admin"), role,
                                            fakes::InMemoryCashLedger::kAdministratorId};
        auto presenter = std::make_unique<presentation::CashBalancesPresenter>(m_view, *m_service, runner, actor);
        presenter->start();
        return presenter;
    }

private slots:
    void init()
    {
        m_ledger = fakes::InMemoryCashLedger();
        m_service = std::make_unique<application::CashLedgerService>(m_ledger);
        m_view = FakeCashBalancesView();
    }

    void showsTheThreeBalances()
    {
        m_ledger.setBalance(CashAccount::Bank, 500000);
        auto presenter = make(m_runner);
        QCOMPARE(m_view.balances.size(), 3);
        QCOMPARE(m_view.balance(CashAccount::Bank), qint64(500000));
        QVERIFY(!m_view.busy);
        QVERIFY(m_view.message.isEmpty());
    }

    void sellerSeesNothing()
    {
        auto presenter = make(m_runner, domain::UserRole::SalesAgent);
        QVERIFY(m_view.balances.isEmpty());
        QVERIFY(!m_view.message.isEmpty());
    }

    // Se confirma antes; guardado, se cierra el formulario y se relee.
    void openingIsConfirmedSavedAndReloaded()
    {
        auto presenter = make(m_runner);
        presenter->onOpeningRequested();
        QCOMPARE(m_view.form, QStringLiteral("apertura"));
        presenter->onSubmitOpening({2000000, 10000000});
        QCOMPARE(m_view.confirmations, 1);
        QVERIFY(m_view.form.isEmpty());
        QCOMPARE(m_view.notice, QStringLiteral("Se capturaron los saldos iniciales."));
        QCOMPARE(m_view.balance(CashAccount::PettyCash), qint64(2000000));
        QCOMPARE(m_view.balance(CashAccount::Bank), qint64(10000000));
        QCOMPARE(m_view.movements.size(), 2);
    }

    void openingNotConfirmedSavesNothing()
    {
        auto presenter = make(m_runner);
        presenter->onOpeningRequested();
        m_view.confirm = false;
        presenter->onSubmitOpening({100, 100});
        QCOMPARE(m_ledger.writes, 0);
        QCOMPARE(m_view.form, QStringLiteral("apertura"));
    }

    // Otro administrador ya los capturó: se cierra, se avisa y el aviso
    // sobrevive a la relectura.
    void openingConflictKeepsItsMessage()
    {
        auto presenter = make(m_runner);
        presenter->onOpeningRequested();
        m_ledger.setBalance(CashAccount::PettyCash, 1);
        presenter->onSubmitOpening({100, 100});
        QVERIFY(m_view.form.isEmpty());
        QVERIFY(m_view.message.contains(QStringLiteral("Otro administrador")));
        QCOMPARE(m_view.balance(CashAccount::PettyCash), qint64(1));
    }

    // El ingreso recibe los saldos para su máximo; uno mayor que el saldo se
    // marca en el formulario, que sigue abierto.
    void depositBeyondTheSourceStaysOpenWithItsError()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        auto presenter = make(m_runner);
        presenter->onDepositRequested();
        QCOMPARE(m_view.form, QStringLiteral("ingreso"));
        QCOMPARE(m_view.depositBalances.size(), 3);
        presenter->onSubmitDeposit({CashAccount::PettyCash, 100001});
        QCOMPARE(m_view.form, QStringLiteral("ingreso"));
        QCOMPARE(m_view.formErrors.size(), 1);
        QCOMPARE(m_view.formErrors.first().field, QStringLiteral("amount"));
        QVERIFY(!m_view.formBusy);
    }

    // Otra sesión gastó el dinero mientras el formulario estaba abierto: se
    // rechaza y se relee, así la tarjeta (y el máximo del formulario) dicen
    // lo vigente.
    void rejectionRereadsTheBalances()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        auto presenter = make(m_runner);
        presenter->onDepositRequested();
        m_ledger.spentMeanwhile[CashAccount::PettyCash] = 60000;
        presenter->onSubmitDeposit({CashAccount::PettyCash, 50000});
        QCOMPARE(m_view.form, QStringLiteral("ingreso"));
        QCOMPARE(m_view.formErrors.size(), 1);
        QCOMPARE(m_view.balance(CashAccount::PettyCash), qint64(40000));
    }

    // Una lectura vieja (de otro filtro) que llega después de la última no
    // pinta encima.
    void staleReadIsDiscarded()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        (void)m_service->depositToSafe({CashAccount::PettyCash, 40000},
                                       {QStringLiteral("admin"), {}, domain::UserRole::Admin,
                                        fakes::InMemoryCashLedger::kAdministratorId});
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        m_view.filter = CashAccount::Safe;
        presenter->onFilterChanged();
        deferred.finish(1);
        QCOMPARE(m_view.movements.size(), 1);
        deferred.finish(0); // la de todas las cuentas, ya vieja
        QCOMPARE(m_view.movements.size(), 1);
    }

    void depositMovesMoneyIntoTheSafe()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        auto presenter = make(m_runner);
        presenter->onDepositRequested();
        presenter->onSubmitDeposit({CashAccount::PettyCash, 40000});
        QVERIFY(m_view.form.isEmpty());
        QCOMPARE(m_view.balance(CashAccount::PettyCash), qint64(60000));
        QCOMPARE(m_view.balance(CashAccount::Safe), qint64(40000));
    }

    // El retiro sabe cuánto hay en Caja Fuerte.
    void withdrawalIsLimitedByTheSafe()
    {
        m_ledger.setBalance(CashAccount::Safe, 300000);
        auto presenter = make(m_runner);
        presenter->onWithdrawalRequested();
        QCOMPARE(m_view.form, QStringLiteral("retiro"));
        QCOMPARE(m_view.withdrawalAvailable, qint64(300000));
        presenter->onSubmitWithdrawal({QDate::currentDate(), QStringLiteral("Nómina"), 100000});
        QVERIFY(m_view.form.isEmpty());
        QCOMPARE(m_view.balance(CashAccount::Safe), qint64(200000));
    }

    // Si falla la base, el formulario sigue abierto con lo capturado.
    void failedWriteKeepsTheForm()
    {
        m_ledger.setBalance(CashAccount::Safe, 300000);
        auto presenter = make(m_runner);
        presenter->onWithdrawalRequested();
        m_ledger.failure = QStringLiteral("se cayó la base");
        presenter->onSubmitWithdrawal({QDate::currentDate(), QStringLiteral("Nómina"), 100000});
        QCOMPARE(m_view.form, QStringLiteral("retiro"));
        QVERIFY(m_view.message.contains(QStringLiteral("se cayó la base")));
    }

    void filterShowsOneAccount()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        auto presenter = make(m_runner);
        presenter->onSubmitDeposit({CashAccount::PettyCash, 40000});
        QCOMPARE(m_view.movements.size(), 2);
        m_view.filter = CashAccount::Safe;
        presenter->onFilterChanged();
        QCOMPARE(m_view.movements.size(), 1);
        QCOMPARE(m_view.movements.first().account, CashAccount::Safe);
    }

    // Mientras guarda no se sale de la página ni se manda otro movimiento.
    void leavingWaitsForTheSave()
    {
        m_ledger.setBalance(CashAccount::PettyCash, 100000);
        fakes::DeferredTaskRunner deferred;
        auto presenter = make(deferred);
        deferred.finish(0);
        QVERIFY(presenter->confirmLeave());
        presenter->onSubmitDeposit({CashAccount::PettyCash, 1000});
        QVERIFY(m_view.formBusy);
        QVERIFY(!presenter->confirmLeave());
        presenter->onSubmitDeposit({CashAccount::PettyCash, 1000});
        deferred.finish(1);
        QVERIFY(presenter->confirmLeave());
        QCOMPARE(m_ledger.writes, 1);
    }

    // Un error de lectura se va cuando una lectura posterior sale bien.
    void successfulReadClearsAnOldError()
    {
        m_ledger.failure = QStringLiteral("sin conexión");
        auto presenter = make(m_runner);
        QVERIFY(m_view.message.contains(QStringLiteral("sin conexión")));
        m_ledger.failure.clear();
        presenter->refresh();
        QVERIFY(m_view.message.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstCashBalancesPresenter)
#include "tst_cashbalancespresenter.moc"
