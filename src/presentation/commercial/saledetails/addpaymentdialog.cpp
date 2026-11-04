#include "presentation/commercial/saledetails/addpaymentdialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTime>
#include <QVBoxLayout>

namespace {

QString money(double amount)
{
    return QLocale(QLocale::Spanish, QLocale::Mexico)
        .toCurrencyString(amount, QStringLiteral("$"), 2);
}

QRegularExpressionValidator *moneyValidator(QObject *parent)
{
    return new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{1,10}(?:\\.[0-9]{1,2})?")), parent);
}

} // namespace

AddPaymentDialog::AddPaymentDialog(const application::CommercialPaymentDto &payment,
                                   QWidget *parent)
    : QDialog(parent)
    , m_payment(payment)
{
    setObjectName(QStringLiteral("addPaymentDialog"));
    setWindowTitle(QStringLiteral("Registrar pago"));
    setModal(true);
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    auto *details = new QGridLayout;
    details->addWidget(new QLabel(QStringLiteral("Folio de venta:"), this), 0, 0);
    m_saleFolioLabel = new QLabel(QString::number(payment.saleFolio), this);
    m_saleFolioLabel->setObjectName(QStringLiteral("saleFolioLabel"));
    details->addWidget(m_saleFolioLabel, 0, 1);
    details->addWidget(new QLabel(QStringLiteral("Número de cuota:"), this), 0, 2);
    m_installmentLabel = new QLabel(QString::number(payment.installmentNumber), this);
    m_installmentLabel->setObjectName(QStringLiteral("installmentNumberLabel"));
    details->addWidget(m_installmentLabel, 0, 3);

    details->addWidget(new QLabel(QStringLiteral("Fecha programada:"), this), 1, 0);
    m_scheduledDateLabel = new QLabel(
        payment.scheduledDate.isValid()
            ? payment.scheduledDate.toString(QStringLiteral("dd/MM/yyyy"))
            : QString(), this);
    m_scheduledDateLabel->setObjectName(QStringLiteral("scheduledDateLabel"));
    details->addWidget(m_scheduledDateLabel, 1, 1);
    details->addWidget(new QLabel(QStringLiteral("Monto esperado:"), this), 1, 2);
    m_expectedAmountLabel = new QLabel(money(payment.expectedAmount), this);
    m_expectedAmountLabel->setObjectName(QStringLiteral("expectedAmountLabel"));
    details->addWidget(m_expectedAmountLabel, 1, 3);
    root->addLayout(details);

    auto *form = new QFormLayout;
    m_paidDate = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_paidDate->setObjectName(QStringLiteral("paidDateEdit"));
    m_paidDate->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_paidDate->setCalendarPopup(true);
    m_paidDate->setDateTimeRange(
        QDateTime(QDate::currentDate().addMonths(-2), QTime(0, 0)),
        QDateTime(QDate::currentDate(), QTime(23, 59, 59)));
    m_paidDate->setDateTime(QDateTime::currentDateTime());
    auto *paidDateField = makeField(m_paidDate, &m_paidDateError);
    m_paidDateError->setObjectName(QStringLiteral("paidDateError"));
    form->addRow(QStringLiteral("Fecha de pago:"), paidDateField);

    m_paidAmount = new QLineEdit(this);
    m_paidAmount->setObjectName(QStringLiteral("paidAmountEdit"));
    m_paidAmount->setValidator(moneyValidator(m_paidAmount));
    m_paidAmount->setPlaceholderText(QStringLiteral("0.00"));
    auto *paidAmountField = makeField(m_paidAmount, &m_paidAmountError);
    m_paidAmountError->setObjectName(QStringLiteral("paidAmountError"));
    form->addRow(QStringLiteral("Monto pagado:"), paidAmountField);

    m_lateFee = new QLineEdit(this);
    m_lateFee->setObjectName(QStringLiteral("lateFeeEdit"));
    m_lateFee->setValidator(moneyValidator(m_lateFee));
    double lateFeeprice = 0.0;
    m_lateFee->setText(QString::number(lateFeeprice, 'f', 2));
    auto *lateFeeField = makeField(m_lateFee, &m_lateFeeError);
    m_lateFeeError->setObjectName(QStringLiteral("lateFeeError"));
    form->addRow(QStringLiteral("Recargo por atraso:"), lateFeeField);

    m_paymentMethod = new QLineEdit(this);
    m_paymentMethod->setObjectName(QStringLiteral("paymentMethodEdit"));
    m_paymentMethod->setMaxLength(30);
    auto *paymentMethodField = makeField(m_paymentMethod, &m_paymentMethodError);
    m_paymentMethodError->setObjectName(QStringLiteral("paymentMethodError"));
    form->addRow(QStringLiteral("Forma de pago:"), paymentMethodField);

    m_status = new QComboBox(this);
    m_status->setObjectName(QStringLiteral("paymentStatusCombo"));
    m_status->addItems({QStringLiteral("Pendiente"), QStringLiteral("Pagado"),
                        QStringLiteral("Parcial"), QStringLiteral("Atrasado")});
    m_status->setCurrentIndex(0);
    auto *statusField = makeField(m_status, &m_statusError);
    m_statusError->setObjectName(QStringLiteral("paymentStatusError"));
    form->addRow(QStringLiteral("Estado:"), statusField);

    m_referenceNumber = new QLineEdit(this);
    m_referenceNumber->setObjectName(QStringLiteral("referenceNumberEdit"));
    m_referenceNumber->setMaxLength(100);
    form->addRow(QStringLiteral("Número de referencia:"), m_referenceNumber);

    m_notes = new QPlainTextEdit(this);
    m_notes->setObjectName(QStringLiteral("paymentNotesEdit"));
    m_notes->setMaximumHeight(80);
    form->addRow(QStringLiteral("Notas:"), m_notes);
    root->addLayout(form);

    m_saveError = new QLabel(this);
    m_saveError->setObjectName(QStringLiteral("saveErrorLabel"));
    m_saveError->setStyleSheet(QStringLiteral("color: #c62828;"));
    m_saveError->setWordWrap(true);
    m_saveError->hide();
    root->addWidget(m_saveError);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    m_acceptButton = new QPushButton(QStringLiteral("Aceptar"), this);
    m_acceptButton->setObjectName(QStringLiteral("acceptPaymentButton"));
    m_cancelButton = new QPushButton(QStringLiteral("Cancelar"), this);
    m_cancelButton->setObjectName(QStringLiteral("cancelPaymentButton"));
    buttons->addWidget(m_acceptButton);
    buttons->addWidget(m_cancelButton);
    root->addLayout(buttons);

    connect(m_acceptButton, &QPushButton::clicked, this, &AddPaymentDialog::submit);
    connect(m_cancelButton, &QPushButton::clicked, this, &AddPaymentDialog::reject);
    connect(m_paidDate, &QDateTimeEdit::dateChanged, this, [this] { m_dirty = true; });
    connect(m_paidAmount, &QLineEdit::textEdited, this, [this] { m_dirty = true; });
    connect(m_lateFee, &QLineEdit::textEdited, this, [this] { m_dirty = true; });
    connect(m_paymentMethod, &QLineEdit::textEdited, this, [this] { m_dirty = true; });
    connect(m_status, &QComboBox::currentIndexChanged, this,
            [this] { m_dirty = true; });
    connect(m_referenceNumber, &QLineEdit::textEdited, this, [this] { m_dirty = true; });
    connect(m_notes, &QPlainTextEdit::textChanged, this, [this] { m_dirty = true; });
}

QWidget *AddPaymentDialog::makeField(QWidget *editor, QLabel **errorLabel)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(editor);
    *errorLabel = new QLabel(container);
    (*errorLabel)->setStyleSheet(QStringLiteral("color: #c62828;"));
    (*errorLabel)->setWordWrap(true);
    (*errorLabel)->hide();
    layout->addWidget(*errorLabel);
    return container;
}

bool AddPaymentDialog::validate()
{
    const auto setError = [](QLabel *label, const QString &message) {
        label->setText(message);
        label->setVisible(!message.isEmpty());
    };
    setError(m_paidDateError, {});
    setError(m_paidAmountError, {});
    setError(m_lateFeeError, {});
    setError(m_paymentMethodError, {});
    setError(m_statusError, {});
    m_saveError->clear();
    m_saveError->hide();

    bool valid = true;
    if (!m_paidDate->date().isValid()
        || m_paidDate->date() < QDate::currentDate().addMonths(-2)
        || m_paidDate->date() > QDate::currentDate()) {
        setError(m_paidDateError, QStringLiteral("Selecciona una fecha entre hoy y los últimos dos meses."));
        valid = false;
    }

    bool amountOk = false;
    const double amount = m_paidAmount->text().toDouble(&amountOk);
    if (m_paidAmount->text().isEmpty() || !m_paidAmount->hasAcceptableInput()
        || !amountOk || amount <= 0.0) {
        setError(m_paidAmountError, QStringLiteral("Ingresa un monto numérico mayor que cero con hasta dos decimales."));
        valid = false;
    }

    bool lateFeeOk = false;
    const double lateFee = m_lateFee->text().toDouble(&lateFeeOk);
    if (m_lateFee->text().isEmpty() || !m_lateFee->hasAcceptableInput()
        || !lateFeeOk || lateFee < 0.0) {
        setError(m_lateFeeError, QStringLiteral("Ingresa un recargo numérico mayor o igual a cero con hasta dos decimales."));
        valid = false;
    }

    if (m_paymentMethod->text().trimmed().isEmpty()) {
        setError(m_paymentMethodError, QStringLiteral("La forma de pago es obligatoria."));
        valid = false;
    }
    if (m_status->currentIndex() < 0) {
        setError(m_statusError, QStringLiteral("Selecciona un estado."));
        valid = false;
    }

    if (!valid) {
        for (QLabel *error : {m_paidDateError, m_paidAmountError, m_lateFeeError,
                              m_paymentMethodError, m_statusError}) {
            if (error->isVisible()) {
                QWidget *field = error->parentWidget()->findChild<QWidget *>();
                if (field)
                    field->setFocus();
                break;
            }
        }
    }
    return valid;
}

void AddPaymentDialog::submit()
{
    if (!validate())
        return;

    m_payment.paidDate = m_paidDate->date();
    m_payment.paidAmount = m_paidAmount->text().toDouble();
    m_payment.lateFee = m_lateFee->text().toDouble();
    m_payment.paymentMethod = m_paymentMethod->text().trimmed();
    m_payment.status = m_status->currentText();
    m_payment.referenceNumber = m_referenceNumber->text().trimmed();
    m_payment.notes = m_notes->toPlainText().trimmed();
    setBusy(true);
    emit submitted(m_payment);
}

void AddPaymentDialog::setBusy(bool busy)
{
    m_acceptButton->setEnabled(!busy);
    m_cancelButton->setEnabled(!busy);
    for (const QString &name : {QStringLiteral("paidDateEdit"),
                                QStringLiteral("paidAmountEdit"),
                                QStringLiteral("lateFeeEdit"),
                                QStringLiteral("paymentMethodEdit"),
                                QStringLiteral("paymentStatusCombo"),
                                QStringLiteral("referenceNumberEdit"),
                                QStringLiteral("paymentNotesEdit")}) {
        if (QWidget *editor = findChild<QWidget *>(name))
            editor->setEnabled(!busy);
    }
}

void AddPaymentDialog::showSaveError(const QString &message)
{
    setBusy(false);
    m_saveError->setText(message);
    m_saveError->show();
}

void AddPaymentDialog::finishSave()
{
    m_discardConfirmed = true;
    QDialog::accept();
}

bool AddPaymentDialog::confirmDiscard()
{
    QMessageBox message(this);
    message.setWindowTitle(QStringLiteral("Cancelar registro de pago"));
    message.setIcon(QMessageBox::Question);
    message.setText(QStringLiteral(
        "¿Estás seguro de cancelar? Se perderá la información capturada."));
    QPushButton *discardButton =
        message.addButton(QStringLiteral("Sí, descartar"), QMessageBox::DestructiveRole);
    message.addButton(QStringLiteral("No, seguir editando"), QMessageBox::RejectRole);
    message.exec();
    return message.clickedButton() == discardButton;
}

void AddPaymentDialog::reject()
{
    if (m_dirty && !m_discardConfirmed && !confirmDiscard())
        return;
    QDialog::reject();
}
