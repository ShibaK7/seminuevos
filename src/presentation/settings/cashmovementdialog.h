#ifndef PRESENTATION_SETTINGS_CASHMOVEMENTDIALOG_H
#define PRESENTATION_SETTINGS_CASHMOVEMENTDIALOG_H

#include "application/finance/dto/cashdtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
    class CashMovementDialog;
}
QT_END_NAMESPACE

// El formulario modal de un movimiento a mano en las cuentas de dinero, en
// tres modos:
//   - Opening: los saldos iniciales de Caja Chica y Cantidad Banco (una vez);
//   - Deposit: ingreso a Caja Fuerte desde Caja Chica o Cantidad Banco;
//   - Withdrawal: retiro de Caja Fuerte, con cuándo salió, el concepto y
//     cuánto.
// En el ingreso y el retiro, el monto no puede pasar de lo que hay en la
// cuenta de la que sale: el campo tiene ese máximo y dice cuánto hay. Su
// formulario es cashmovementdialog.ui, en esta misma carpeta.
//
// Vista pasiva: Aceptar no valida ni cierra, solo avisa (submitted). Quien
// guarda decide si cierra (accept()) o marca los errores (showErrors()).
// Cancelar lo cierra, salvo mientras guarda.
class CashMovementDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Opening, Deposit, Withdrawal };

    // `balances`: lo que hay en cada cuenta, para el máximo y el aviso.
    CashMovementDialog(Mode mode, const QList<application::CashBalanceDto> &balances,
                       QWidget *parent = nullptr);
    ~CashMovementDialog() override;

    Mode mode() const;
    application::OpeningBalancesDto opening() const;
    application::SafeDepositDto deposit() const;
    application::SafeWithdrawalDto withdrawal() const;

    void setBusy(bool busy);
    // Lo que hay ahora en cada cuenta (se releyó): ajusta el máximo y el
    // aviso de lo disponible.
    void setBalances(const QList<application::CashBalanceDto> &balances);
    // Marca cada campo con su error y le da el foco al primero.
    void showErrors(const QList<domain::ValidationError> &errors);

public slots:
    void reject() override;

signals:
    // Aceptar.
    void submitted();

private:
    // Lo que hay en la cuenta de la que sale el dinero, como máximo del
    // monto y como aviso bajo él.
    void applyAvailable();
    qint64 balanceOf(domain::CashAccount account) const;

    Ui::CashMovementDialog *ui;
    bool m_busy = false;
    Mode m_mode;
    QList<application::CashBalanceDto> m_balances;
};

#endif // PRESENTATION_SETTINGS_CASHMOVEMENTDIALOG_H
