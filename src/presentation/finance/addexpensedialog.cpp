#include "presentation/finance/addexpensedialog.h"
#include "ui_addexpensedialog.h"

#include "domain/finance/value_objects/expenseamounts.h"
#include "presentation/common/forms/formsupport.h"

#include <QCheckBox>
#include <QDate>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

namespace {

// La descripción de cada tipo va junto a su id en el combo.
constexpr int kDescriptionRole = Qt::UserRole + 1;

QString money(qint64 cents)
{
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    return QStringLiteral("$%1").arg(locale.toString(cents / 100.0, 'f', 2));
}

} // namespace

AddExpenseDialog::AddExpenseDialog(const QList<application::ExpenseTypeOptionDto> &expenseTypes,
                                   QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddExpenseDialog)
{
    ui->setupUi(this);
    formsupport::addFieldErrorLabels(this);

    // Hoy depende del día en que se abre: no cabe en el .ui.
    ui->expenseDate->setDate(QDate::currentDate());

    // Sin elegir de entrada: suponer un tipo lo guardaría sin que nadie lo
    // decidiera.
    for (const application::ExpenseTypeOptionDto &type : expenseTypes) {
        ui->expenseTypeCombo->addItem(type.name, type.id);
        ui->expenseTypeCombo->setItemData(ui->expenseTypeCombo->count() - 1, type.description,
                                          kDescriptionRole);
    }
    ui->expenseTypeCombo->setPlaceholderText(QStringLiteral("Seleccione una opción..."));
    ui->expenseTypeCombo->setCurrentIndex(-1);

    // Valores cerrados: salen del dominio, que sigue al CHECK de la columna.
    for (domain::PaymentMethod value : domain::allPaymentMethods())
        ui->paymentMethodCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    ui->paymentMethodCombo->setPlaceholderText(QStringLiteral("Seleccione una opción..."));
    ui->paymentMethodCombo->setCurrentIndex(-1);

    // No. Factura solo aparece con la casilla marcada. Se oculta aquí y no en
    // el .ui para que Designer lo siga mostrando al editarlo.
    const auto showInvoiceNumber = [this](bool shown) {
        ui->invoiceNumberLabel->setVisible(shown);
        ui->invoiceNumber->setVisible(shown);
    };
    showInvoiceNumber(false);
    connect(ui->invoiceCheck, &QCheckBox::toggled, this, showInvoiceNumber);

    connect(ui->expenseTypeCombo, &QComboBox::currentIndexChanged, this,
            &AddExpenseDialog::showDescriptionOfSelectedType);
    connect(ui->subtotal, &QLineEdit::textChanged, this, &AddExpenseDialog::showAmounts);

    connect(ui->expenseAcceptButton, &QPushButton::clicked, this,
            [this] { emit submitted(expense()); });
    connect(ui->expenseCancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

AddExpenseDialog::~AddExpenseDialog()
{
    delete ui;
}

application::NewOperationalExpenseDto AddExpenseDialog::expense() const
{
    application::NewOperationalExpenseDto dto;
    dto.date = ui->expenseDate->date();
    if (ui->expenseTypeCombo->currentIndex() >= 0)
        dto.expenseTypeId = ui->expenseTypeCombo->currentData().toInt();
    dto.hasInvoice = ui->invoiceCheck->isChecked();
    if (dto.hasInvoice)
        dto.invoiceNumber = ui->invoiceNumber->text();
    dto.subtotal = ui->subtotal->text();
    if (ui->paymentMethodCombo->currentIndex() >= 0)
        dto.paymentMethod =
            static_cast<domain::PaymentMethod>(ui->paymentMethodCombo->currentData().toInt());
    return dto;
}

void AddExpenseDialog::setBusy(bool busy)
{
    ui->expenseAcceptButton->setEnabled(!busy);
}

void AddExpenseDialog::showErrors(const QList<domain::ValidationError> &errors)
{
    const QList<domain::ValidationError> unshown = formsupport::showFieldErrors(this, errors);
    for (const domain::ValidationError &error : errors) {
        if (formsupport::focusField(this, error.field))
            break;
    }
    if (!unshown.isEmpty()) {
        QStringList messages;
        for (const domain::ValidationError &error : unshown)
            messages << error.message;
        QMessageBox::warning(this, QStringLiteral("No se guardó el gasto"),
                             messages.join(QStringLiteral("\n")));
    }
}

void AddExpenseDialog::showDescriptionOfSelectedType()
{
    ui->expenseDescription->setPlainText(
        ui->expenseTypeCombo->currentData(kDescriptionRole).toString());
}

void AddExpenseDialog::showAmounts()
{
    // La cuenta es del dominio. Mientras el subtotal no es un importe válido
    // no se muestra nada; el error sale al presionar Aceptar.
    const std::optional<domain::ExpenseAmounts> amounts =
        domain::ExpenseAmounts::fromSubtotal(ui->subtotal->text());
    ui->ivaValue->setText(amounts ? money(amounts->ivaCents()) : QString());
    ui->totalValue->setText(amounts ? money(amounts->totalCents()) : QString());
}
