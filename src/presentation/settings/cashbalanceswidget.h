#ifndef PRESENTATION_SETTINGS_CASHBALANCESWIDGET_H
#define PRESENTATION_SETTINGS_CASHBALANCESWIDGET_H

#include "presentation/settings/icashbalancesview.h"

#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class CashBalancesWidget;
}
QT_END_NAMESPACE

class CashMovementDialog;
class QLabel;

namespace presentation {
class CashBalancesPresenter;
}

// La tarjeta de saldos de Configuraciones: Caja Chica, Cantidad Banco y Caja
// Fuerte, en ese orden, cada uno con su saldo vigente y la fecha de su último
// movimiento; los botones para capturar los saldos iniciales (solo mientras
// falten), ingresar a Caja Fuerte y retirar de ella; y la tabla de
// movimientos, filtrable por cuenta. Su formulario es cashbalanceswidget.ui,
// en esta misma carpeta.
//
// Vista pasiva de CashBalancesPresenter. El formulario de cada movimiento es
// CashMovementDialog.
class CashBalancesWidget : public QWidget, public presentation::ICashBalancesView
{
    Q_OBJECT

public:
    enum Column { DateColumn, AccountColumn, ConceptColumn, InflowColumn, OutflowColumn, UserColumn };

    explicit CashBalancesWidget(QWidget *parent = nullptr);
    ~CashBalancesWidget() override;

    void bind(presentation::CashBalancesPresenter &presenter);

    // --- ICashBalancesView ---
    void setBusy(bool busy) override;
    void showBalances(const QList<application::CashBalanceDto> &balances) override;
    void showMovements(const QList<application::CashMovementDto> &movements) override;
    std::optional<domain::CashAccount> movementFilter() const override;
    void askOpening() override;
    void askSafeDeposit(const QList<application::CashBalanceDto> &balances) override;
    void askSafeWithdrawal(qint64 availableHundredths) override;
    bool confirmOpening(const application::OpeningBalancesDto &openings) override;
    void setFormBusy(bool busy) override;
    void showFormErrors(const QList<domain::ValidationError> &errors) override;
    void closeForm() override;
    void showMessage(const QString &message) override;
    void showNotice(const QString &notice) override;

signals:
    void filterChanged();
    void openingRequested();
    void depositRequested();
    void withdrawalRequested();
    void openingSubmitted(const application::OpeningBalancesDto &openings);
    void depositSubmitted(const application::SafeDepositDto &deposit);
    void withdrawalSubmitted(const application::SafeWithdrawalDto &withdrawal);

private:
    void openForm(CashMovementDialog *form);
    void applyButtons();
    QLabel *amountLabelOf(domain::CashAccount account) const;
    QLabel *updatedLabelOf(domain::CashAccount account) const;

    Ui::CashBalancesWidget *ui;
    QList<application::CashBalanceDto> m_balances;
    // El formulario abierto, si hay uno. Se borra solo al cerrarse.
    QPointer<CashMovementDialog> m_form;
    bool m_busy = false;
};

#endif // PRESENTATION_SETTINGS_CASHBALANCESWIDGET_H
