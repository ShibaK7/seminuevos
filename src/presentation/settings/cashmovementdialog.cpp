#include "presentation/settings/cashmovementdialog.h"
#include "ui_cashmovementdialog.h"

#include "presentation/common/forms/formsupport.h"

#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

#include <cmath>

namespace {

qint64 hundredthsOf(double pesos)
{
    return std::llround(pesos * 100.0);
}

QString pesos(qint64 hundredths)
{
    return QStringLiteral("$%1").arg(
        QLocale(QLocale::Spanish, QLocale::Mexico).toString(static_cast<double>(hundredths) / 100.0, 'f', 2));
}

} // namespace

CashMovementDialog::CashMovementDialog(Mode mode, const QList<application::CashBalanceDto> &balances,
                                       QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CashMovementDialog)
    , m_mode(mode)
    , m_balances(balances)
{
    ui->setupUi(this);
    formsupport::addFieldErrorLabels(this);

    // El origen de un ingreso: Caja Chica o Cantidad Banco, en ese orden.
    for (const domain::CashAccount account : domain::allCashAccounts()) {
        if (account != domain::CashAccount::Safe)
            ui->sourceCombo->addItem(domain::displayLabel(account), static_cast<int>(account));
    }
    ui->dateEdit->setDate(QDate::currentDate());
    ui->dateEdit->setMaximumDate(QDate::currentDate());

    const bool opening = mode == Mode::Opening;
    const bool deposit = mode == Mode::Deposit;
    const bool withdrawal = mode == Mode::Withdrawal;
    ui->movementForm->setRowVisible(ui->pettyCashOpeningSpin, opening);
    ui->movementForm->setRowVisible(ui->bankOpeningSpin, opening);
    ui->movementForm->setRowVisible(ui->sourceCombo, deposit);
    ui->movementForm->setRowVisible(ui->dateEdit, withdrawal);
    ui->movementForm->setRowVisible(ui->conceptEdit, withdrawal);
    ui->movementForm->setRowVisible(ui->amountSpin, !opening);
    ui->movementForm->setRowVisible(ui->availableLabel, !opening);
    switch (mode) {
    case Mode::Opening:
        setWindowTitle(QStringLiteral("Saldos iniciales"));
        ui->titleLabel->setText(QStringLiteral("Saldos iniciales"));
        ui->hintLabel->setText(QStringLiteral("Lo que hay hoy en cada cuenta. Se captura una sola "
                                              "vez: después cada operación mueve el saldo. Caja "
                                              "Fuerte empieza en cero."));
        break;
    case Mode::Deposit:
        setWindowTitle(QStringLiteral("Ingresar a Caja Fuerte"));
        ui->titleLabel->setText(QStringLiteral("Ingresar a Caja Fuerte"));
        ui->hintLabel->setText(QStringLiteral("El dinero sale de la cuenta que elijas y entra a "
                                              "Caja Fuerte."));
        break;
    case Mode::Withdrawal:
        setWindowTitle(QStringLiteral("Retirar de Caja Fuerte"));
        ui->titleLabel->setText(QStringLiteral("Retirar de Caja Fuerte"));
        ui->hintLabel->setText(QStringLiteral("El dinero sale de Caja Fuerte."));
        break;
    }

    applyAvailable();
    connect(ui->sourceCombo, &QComboBox::currentIndexChanged, this, &CashMovementDialog::applyAvailable);
    connect(ui->acceptButton, &QPushButton::clicked, this, &CashMovementDialog::submitted);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

CashMovementDialog::~CashMovementDialog()
{
    delete ui;
}

CashMovementDialog::Mode CashMovementDialog::mode() const
{
    return m_mode;
}

qint64 CashMovementDialog::balanceOf(domain::CashAccount account) const
{
    for (const application::CashBalanceDto &balance : m_balances) {
        if (balance.account == account)
            return balance.hundredths;
    }
    return 0;
}

void CashMovementDialog::applyAvailable()
{
    if (m_mode == Mode::Opening)
        return;
    const domain::CashAccount from =
        m_mode == Mode::Withdrawal
            ? domain::CashAccount::Safe
            : static_cast<domain::CashAccount>(ui->sourceCombo->currentData().toInt());
    const qint64 available = balanceOf(from);
    // No se puede capturar más de lo que hay.
    ui->amountSpin->setMaximum(static_cast<double>(available) / 100.0);
    ui->availableLabel->setText(
        QStringLiteral("Disponible en %1: %2").arg(domain::displayLabel(from), pesos(available)));
}

application::OpeningBalancesDto CashMovementDialog::opening() const
{
    application::OpeningBalancesDto opening;
    opening.pettyCashHundredths = hundredthsOf(ui->pettyCashOpeningSpin->value());
    opening.bankHundredths = hundredthsOf(ui->bankOpeningSpin->value());
    return opening;
}

application::SafeDepositDto CashMovementDialog::deposit() const
{
    application::SafeDepositDto deposit;
    deposit.source = static_cast<domain::CashAccount>(ui->sourceCombo->currentData().toInt());
    deposit.hundredths = hundredthsOf(ui->amountSpin->value());
    return deposit;
}

application::SafeWithdrawalDto CashMovementDialog::withdrawal() const
{
    application::SafeWithdrawalDto withdrawal;
    withdrawal.date = ui->dateEdit->date();
    withdrawal.concept = ui->conceptEdit->text();
    withdrawal.hundredths = hundredthsOf(ui->amountSpin->value());
    return withdrawal;
}

void CashMovementDialog::setBusy(bool busy)
{
    m_busy = busy;
    ui->acceptButton->setEnabled(!busy);
    ui->cancelButton->setEnabled(!busy);
}

void CashMovementDialog::reject()
{
    // Mientras guarda no se cierra (Cancelar, Esc o la X pasan por aquí): el
    // resultado necesita el formulario abierto para tener sentido.
    if (m_busy)
        return;
    QDialog::reject();
}

void CashMovementDialog::showErrors(const QList<domain::ValidationError> &errors)
{
    const QList<domain::ValidationError> unshown = formsupport::showFieldErrors(this, errors);
    // Un mensaje largo ocupa varias líneas: el diálogo crece para que se lea
    // completo.
    layout()->activate();
    if (layout()->hasHeightForWidth())
        resize(width(), qMax(height(), layout()->totalHeightForWidth(width())));
    for (const domain::ValidationError &error : errors) {
        if (formsupport::focusField(this, error.field))
            break;
    }
    if (!unshown.isEmpty()) {
        QStringList messages;
        for (const domain::ValidationError &error : unshown)
            messages << error.message;
        QMessageBox::warning(this, windowTitle(), messages.join(QStringLiteral("\n")));
    }
}
