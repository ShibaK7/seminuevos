#include "presentation/settings/cashbalanceswidget.h"
#include "ui_cashbalanceswidget.h"

#include "presentation/common/forms/formsupport.h"
#include "presentation/settings/cashbalancespresenter.h"
#include "presentation/settings/cashmovementdialog.h"

#include <QHeaderView>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidgetItem>

namespace {

// El filtro "todas las cuentas" en el combo.
constexpr int kAllAccounts = -1;

QString pesos(qint64 hundredths)
{
    return QStringLiteral("$%1").arg(
        QLocale(QLocale::Spanish, QLocale::Mexico).toString(static_cast<double>(hundredths) / 100.0, 'f', 2));
}

QTableWidgetItem *cell(const QString &text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setTextAlignment(alignment);
    return item;
}

} // namespace

CashBalancesWidget::CashBalancesWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CashBalancesWidget)
{
    ui->setupUi(this);
    // La sombra no cabe en el .ui: Designer no la declara.
    formsupport::applyFloatingShadow(ui->balancesCard);
    ui->movementsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->movementsTable->horizontalHeader()->setSectionResizeMode(ConceptColumn, QHeaderView::Stretch);

    ui->filterCombo->addItem(QStringLiteral("Todas"), kAllAccounts);
    for (const domain::CashAccount account : domain::allCashAccounts())
        ui->filterCombo->addItem(domain::displayLabel(account), static_cast<int>(account));

    connect(ui->openingButton, &QPushButton::clicked, this, &CashBalancesWidget::openingRequested);
    connect(ui->depositButton, &QPushButton::clicked, this, &CashBalancesWidget::depositRequested);
    connect(ui->withdrawButton, &QPushButton::clicked, this, &CashBalancesWidget::withdrawalRequested);
    connect(ui->filterCombo, &QComboBox::currentIndexChanged, this, &CashBalancesWidget::filterChanged);

    showBalances({});
    showMessage(QString());
    showNotice(QString());
}

CashBalancesWidget::~CashBalancesWidget()
{
    delete ui;
}

void CashBalancesWidget::bind(presentation::CashBalancesPresenter &presenter)
{
    using presentation::CashBalancesPresenter;
    connect(this, &CashBalancesWidget::filterChanged, &presenter, &CashBalancesPresenter::onFilterChanged);
    connect(this, &CashBalancesWidget::openingRequested, &presenter,
            &CashBalancesPresenter::onOpeningRequested);
    connect(this, &CashBalancesWidget::depositRequested, &presenter,
            &CashBalancesPresenter::onDepositRequested);
    connect(this, &CashBalancesWidget::withdrawalRequested, &presenter,
            &CashBalancesPresenter::onWithdrawalRequested);
    connect(this, &CashBalancesWidget::openingSubmitted, &presenter, &CashBalancesPresenter::onSubmitOpening);
    connect(this, &CashBalancesWidget::depositSubmitted, &presenter, &CashBalancesPresenter::onSubmitDeposit);
    connect(this, &CashBalancesWidget::withdrawalSubmitted, &presenter,
            &CashBalancesPresenter::onSubmitWithdrawal);
}

QLabel *CashBalancesWidget::amountLabelOf(domain::CashAccount account) const
{
    switch (account) {
    case domain::CashAccount::PettyCash:
        return ui->pettyCashAmount;
    case domain::CashAccount::Bank:
        return ui->bankAmount;
    case domain::CashAccount::Safe:
        return ui->safeAmount;
    }
    return nullptr;
}

QLabel *CashBalancesWidget::updatedLabelOf(domain::CashAccount account) const
{
    switch (account) {
    case domain::CashAccount::PettyCash:
        return ui->pettyCashUpdated;
    case domain::CashAccount::Bank:
        return ui->bankUpdated;
    case domain::CashAccount::Safe:
        return ui->safeUpdated;
    }
    return nullptr;
}

void CashBalancesWidget::setBusy(bool busy)
{
    m_busy = busy;
    applyButtons();
}

void CashBalancesWidget::showBalances(const QList<application::CashBalanceDto> &balances)
{
    m_balances = balances;
    const QLocale mexico(QLocale::Spanish, QLocale::Mexico);
    for (const domain::CashAccount account : domain::allCashAccounts()) {
        application::CashBalanceDto balance;
        balance.account = account;
        for (const application::CashBalanceDto &row : balances) {
            if (row.account == account)
                balance = row;
        }
        amountLabelOf(account)->setText(pesos(balance.hundredths));
        // La base guarda la hora en UTC.
        updatedLabelOf(account)->setText(
            !balance.opened && account != domain::CashAccount::Safe
                ? QStringLiteral("Sin saldo inicial")
            : balance.updatedAt.isValid()
                ? QStringLiteral("Actualizado el %1")
                      .arg(mexico.toString(balance.updatedAt.toLocalTime().date(),
                                           QStringLiteral("d 'de' MMMM 'de' yyyy")))
                : QStringLiteral("Sin movimientos"));
    }
    applyButtons();
}

void CashBalancesWidget::applyButtons()
{
    bool missingOpening = false;
    qint64 sources = 0;
    qint64 safe = 0;
    for (const application::CashBalanceDto &balance : std::as_const(m_balances)) {
        if (balance.account == domain::CashAccount::Safe)
            safe = balance.hundredths;
        else {
            sources += balance.hundredths;
            missingOpening = missingOpening || !balance.opened;
        }
    }
    // Hasta que se lean, no se ofrece nada.
    const bool loaded = !m_balances.isEmpty();
    ui->openingButton->setVisible(loaded && missingOpening);
    ui->openingButton->setEnabled(!m_busy);
    ui->depositButton->setEnabled(loaded && !m_busy && sources > 0);
    ui->depositButton->setToolTip(loaded && sources <= 0
                                      ? QStringLiteral("No hay dinero en Caja Chica ni en Cantidad Banco.")
                                      : QString());
    ui->withdrawButton->setEnabled(loaded && !m_busy && safe > 0);
    ui->withdrawButton->setToolTip(loaded && safe <= 0 ? QStringLiteral("Caja Fuerte está vacía.")
                                                       : QString());
}

void CashBalancesWidget::showMovements(const QList<application::CashMovementDto> &movements)
{
    const QLocale mexico(QLocale::Spanish, QLocale::Mexico);
    const Qt::Alignment right = Qt::AlignRight | Qt::AlignVCenter;
    ui->movementsTable->setRowCount(0);
    ui->movementsTable->setRowCount(movements.size());
    for (int row = 0; row < movements.size(); ++row) {
        const application::CashMovementDto &movement = movements.at(row);
        ui->movementsTable->setItem(row, DateColumn,
                                    cell(mexico.toString(movement.date, QStringLiteral("dd/MM/yyyy"))));
        ui->movementsTable->setItem(row, AccountColumn, cell(domain::displayLabel(movement.account)));
        ui->movementsTable->setItem(row, ConceptColumn, cell(movement.concept));
        ui->movementsTable->setItem(row, InflowColumn,
                                    cell(movement.hundredths > 0 ? pesos(movement.hundredths) : QString(), right));
        ui->movementsTable->setItem(row, OutflowColumn,
                                    cell(movement.hundredths < 0 ? pesos(-movement.hundredths) : QString(), right));
        // Sin usuario: lo movió una operación (una compra, una venta).
        ui->movementsTable->setItem(row, UserColumn,
                                    cell(movement.userName.isEmpty() ? QStringLiteral("—") : movement.userName));
    }
}

std::optional<domain::CashAccount> CashBalancesWidget::movementFilter() const
{
    const int data = ui->filterCombo->currentData().toInt();
    if (data == kAllAccounts)
        return std::nullopt;
    return static_cast<domain::CashAccount>(data);
}

void CashBalancesWidget::openForm(CashMovementDialog *form)
{
    if (m_form)
        m_form->close();
    m_form = form;
    m_form->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_form, &CashMovementDialog::submitted, this, [this] {
        if (!m_form)
            return;
        switch (m_form->mode()) {
        case CashMovementDialog::Mode::Opening:
            emit openingSubmitted(m_form->opening());
            break;
        case CashMovementDialog::Mode::Deposit:
            emit depositSubmitted(m_form->deposit());
            break;
        case CashMovementDialog::Mode::Withdrawal:
            emit withdrawalSubmitted(m_form->withdrawal());
            break;
        }
    });
    // open() y no exec(): modal igual, pero sin un bucle de eventos anidado
    // mientras el presenter guarda en otro hilo.
    m_form->open();
}

void CashBalancesWidget::askOpening()
{
    // Hijo de la ventana y no de esta tarjeta, como el formulario de
    // usuarios: nadie que busque campos por su clave en la página debe
    // encontrar los del formulario.
    openForm(new CashMovementDialog(CashMovementDialog::Mode::Opening, m_balances, window()));
}

void CashBalancesWidget::askSafeDeposit(const QList<application::CashBalanceDto> &balances)
{
    openForm(new CashMovementDialog(CashMovementDialog::Mode::Deposit, balances, window()));
}

void CashBalancesWidget::askSafeWithdrawal(qint64 availableHundredths)
{
    application::CashBalanceDto safe;
    safe.account = domain::CashAccount::Safe;
    safe.hundredths = availableHundredths;
    safe.opened = true;
    openForm(new CashMovementDialog(CashMovementDialog::Mode::Withdrawal, {safe}, window()));
}

bool CashBalancesWidget::confirmOpening(const application::OpeningBalancesDto &openings)
{
    QWidget *parent = m_form ? static_cast<QWidget *>(m_form.data()) : this;
    const QString question =
        QStringLiteral("¿Capturar Caja Chica con %1 y Cantidad Banco con %2?\n\nLos saldos iniciales "
                       "se capturan una sola vez: después solo los mueven las operaciones.")
            .arg(pesos(openings.pettyCashHundredths), pesos(openings.bankHundredths));
    return QMessageBox::question(parent, QStringLiteral("Saldos iniciales"), question,
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

void CashBalancesWidget::setFormBusy(bool busy)
{
    if (m_form)
        m_form->setBusy(busy);
}

void CashBalancesWidget::showFormErrors(const QList<domain::ValidationError> &errors)
{
    if (m_form) {
        m_form->showErrors(errors);
        return;
    }
    QStringList messages;
    for (const domain::ValidationError &error : errors)
        messages << error.message;
    showMessage(messages.join(QLatin1Char(' ')));
}

void CashBalancesWidget::closeForm()
{
    // Se suelta ya, no cuando se borre: un aviso que llega justo después no
    // debe salir sobre un formulario cerrado.
    if (CashMovementDialog *form = m_form.data()) {
        m_form.clear();
        form->accept();
    }
}

void CashBalancesWidget::showMessage(const QString &message)
{
    ui->messageLabel->setText(message);
    ui->messageLabel->setVisible(!message.isEmpty());
    // Con el formulario abierto, el mensaje se vería detrás de él.
    if (m_form && !message.isEmpty())
        QMessageBox::warning(m_form, m_form->windowTitle(), message);
}

void CashBalancesWidget::showNotice(const QString &notice)
{
    ui->noticeLabel->setText(notice);
    ui->noticeLabel->setVisible(!notice.isEmpty());
}
