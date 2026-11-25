#include "presentation/finance/financeview.h"
#include "ui_financeview.h"

#include "presentation/common/forms/formsupport.h"
#include "presentation/finance/addexpensedialog.h"
#include "presentation/finance/financepresenter.h"

#include <QAbstractItemDelegate>
#include <QComboBox>
#include <QDate>
#include <QLabel>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHeaderView>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidgetItem>
#include <QUrl>

namespace {

// El folio de operational_expenses de cada renglón va en la celda del folio.
constexpr int kFolioRole = Qt::UserRole;
// La celda No. Factura de un gasto sin factura muestra "Sin Factura", con el
// color de un placeholder: no es un número.
constexpr int kPlaceholderRole = Qt::UserRole + 1;

QTableWidgetItem *cell(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

QTableWidgetItem *moneyCell(const QLocale &locale, double amount)
{
    QTableWidgetItem *item = cell(QStringLiteral("$%1").arg(locale.toString(amount, 'f', 2)));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

} // namespace

FinanceView::FinanceView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FinanceView)
{
    ui->setupUi(this);

    // La única pestaña del módulo arranca seleccionada. Va aquí y no como
    // `checked` en el .ui: Designer ve los promovidos como QPushButton no
    // checkable y al guardar lo reescribe como false.
    ui->generalExpensesTab->setChecked(true);

    // La sombra de las tarjetas no cabe en el .ui: Designer no la puede
    // declarar.
    for (QWidget *card : {static_cast<QWidget *>(ui->totalExpensesCard),
                          static_cast<QWidget *>(ui->bankCard),
                          static_cast<QWidget *>(ui->pettyCashCard),
                          static_cast<QWidget *>(ui->expensesCard)})
        formsupport::applyFloatingShadow(card);

    // La descripción es lo único de largo libre: se lleva el ancho que sobre.
    // Las demás columnas se ajustan a su contenido.
    QHeaderView *header = ui->expensesTable->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(DescriptionColumn, QHeaderView::Stretch);

    connect(ui->addExpenseButton, &QPushButton::clicked, this, &FinanceView::addExpenseRequested);
    connect(ui->printReceiptButton, &QPushButton::clicked, this,
            &FinanceView::printReceiptRequested);

    // Los meses del Periodo: "Todo" va primero en el .ui, así que el índice
    // de cada mes es su número (Enero = 1).
    for (int month = 1; month < ui->periodCombo->count(); ++month)
        ui->periodCombo->setItemData(month, month);
    ui->periodCombo->setCurrentIndex(0);
    // El Año arranca solo con "Todo"; los años llegan con la primera lectura.
    ui->yearCombo->addItem(QStringLiteral("Todo"));
    for (QComboBox *combo : {ui->periodCombo, ui->yearCombo})
        connect(combo, &QComboBox::currentIndexChanged, this,
                [this] { emit filterChanged(filter()); });

    // El aviso sale en la siguiente vuelta del bucle de eventos: la tabla
    // todavía está cerrando el editor, y el presenter puede repintarla.
    connect(ui->expensesTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_painting)
            return;
        const int row = item->row();
        const QTableWidgetItem *folioCell = ui->expensesTable->item(row, FolioColumn);
        const QTableWidgetItem *invoice = ui->expensesTable->item(row, InvoiceColumn);
        if (!folioCell || !invoice)
            return;
        const int folio = folioCell->data(kFolioRole).toInt();

        if (item->column() == InvoiceColumn) {
            syncInvoiceNumberEditable(row);
            const bool checked = item->checkState() == Qt::Checked;
            QMetaObject::invokeMethod(
                this, [this, folio, checked] { emit invoiceToggled(folio, checked); },
                Qt::QueuedConnection);
        }
    });

    // El No. Factura se avisa al cerrar su editor y no en itemChanged: así
    // también llega cuando se sale sin escribir nada (Esc, o Enter con la
    // celda vacía), que es cuando el presenter alerta que falta el número.
    // El delegado ya guardó el texto en la celda antes de cerrar el editor.
    connect(ui->expensesTable->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this] {
        const QTableWidgetItem *number = ui->expensesTable->currentItem();
        if (!number || number->column() != InvoiceNumberColumn)
            return;
        const int row = number->row();
        const QTableWidgetItem *folioCell = ui->expensesTable->item(row, FolioColumn);
        const QTableWidgetItem *invoice = ui->expensesTable->item(row, InvoiceColumn);
        if (!folioCell || !invoice || invoice->checkState() != Qt::Checked)
            return;
        const int folio = folioCell->data(kFolioRole).toInt();
        const QString text = number->text();
        QMetaObject::invokeMethod(
            this, [this, folio, text] { emit invoiceNumberEdited(folio, text); },
            Qt::QueuedConnection);
    });
}

FinanceView::~FinanceView()
{
    delete ui;
}

void FinanceView::bind(presentation::FinancePresenter &presenter)
{
    using presentation::FinancePresenter;
    connect(this, &FinanceView::addExpenseRequested, &presenter, &FinancePresenter::onAddExpense);
    connect(this, &FinanceView::printReceiptRequested, &presenter,
            &FinancePresenter::onPrintReceipt);
    connect(this, &FinanceView::filterChanged, &presenter, &FinancePresenter::onFilterChanged);
    connect(this, &FinanceView::expenseSubmitted, &presenter, &FinancePresenter::onSubmitExpense);
    connect(this, &FinanceView::invoiceToggled, &presenter, &FinancePresenter::onInvoiceToggled);
    connect(this, &FinanceView::invoiceNumberEdited, &presenter,
            &FinancePresenter::onInvoiceNumberEdited);
}

void FinanceView::setBusy(bool busy)
{
    ui->addExpenseButton->setEnabled(!busy);
}

void FinanceView::showExpenses(const QList<application::OperationalExpenseItemDto> &items)
{
    const QScopedValueRollback<bool> painting(m_painting, true);
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    QTableWidget *table = ui->expensesTable;
    table->setRowCount(0);
    table->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const application::OperationalExpenseItemDto &item = items.at(row);
        QTableWidgetItem *folio = cell(QString::number(item.folio));
        folio->setData(kFolioRole, item.folio);
        table->setItem(row, FolioColumn, folio);
        table->setItem(row, DateColumn, cell(item.date.toString(QStringLiteral("dd/MM/yyyy"))));
        table->setItem(row, ExpenseTypeColumn, cell(item.typeName));
        table->setItem(row, DescriptionColumn, cell(item.description));

        QTableWidgetItem *invoice = cell(QString());
        invoice->setFlags(invoice->flags() | Qt::ItemIsUserCheckable);
        invoice->setCheckState(item.hasInvoice ? Qt::Checked : Qt::Unchecked);
        table->setItem(row, InvoiceColumn, invoice);
        QTableWidgetItem *number = cell(item.invoiceNumber);
        if (!item.hasInvoice) {
            number->setText(QStringLiteral("Sin Factura"));
            number->setData(kPlaceholderRole, true);
            number->setForeground(table->palette().color(QPalette::PlaceholderText));
        }
        table->setItem(row, InvoiceNumberColumn, number);
        syncInvoiceNumberEditable(row);

        table->setItem(row, SubtotalColumn, moneyCell(locale, item.subtotal));
        table->setItem(row, IvaColumn, moneyCell(locale, item.iva));
        table->setItem(row, TotalColumn, moneyCell(locale, item.total));
        table->setItem(
            row, PaymentMethodColumn,
            cell(item.paymentMethod ? domain::displayLabel(*item.paymentMethod) : QString()));
    }
}

void FinanceView::showYears(const QList<int> &years)
{
    QComboBox *combo = ui->yearCombo;
    // "Todo" no lleva dato: un año elegido sí.
    const QVariant chosen = combo->currentData();
    int index = 0;
    {
        // Repintar no es un cambio del usuario: no dispara el filtro.
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(QStringLiteral("Todo"));
        for (int year : years)
            combo->addItem(QString::number(year), year);
        if (chosen.isValid())
            index = combo->findData(chosen);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    }
    // El año elegido ya no tiene gastos: el combo volvió a "Todo" y la tabla
    // debe seguirlo.
    if (index < 0)
        emit filterChanged(filter());
}

void FinanceView::showMonthlySummary(const application::MonthlyExpenseSummaryDto &summary)
{
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    QString month = locale.standaloneMonthName(summary.month);
    if (!month.isEmpty())
        month[0] = month.at(0).toUpper();
    // El año solo cuando no es el actual: "Noviembre", pero "Marzo 2025".
    if (summary.year != QDate::currentDate().year())
        month += QStringLiteral(" %1").arg(summary.year);
    ui->totalExpensesTitle->setText(QStringLiteral("Total Egresos (%1)").arg(month));
    ui->totalExpensesAmount->setText(
        QStringLiteral("$%1").arg(locale.toString(summary.total, 'f', 2)));

    // En egresos subir es malo: "up" se pinta en rojo y "down" en verde (QSS).
    QString trend = QStringLiteral("flat");
    QString text;
    if (!summary.changePercent) {
        text = QStringLiteral("↑ Sin gastos el mes anterior");
        trend = QStringLiteral("up");
    } else {
        const double change = *summary.changePercent;
        const QString percent = locale.toString(qAbs(change), 'f', 2);
        if (change >= 0.005) {
            text = QStringLiteral("↑ %1%").arg(percent);
            trend = QStringLiteral("up");
        } else if (change <= -0.005) {
            text = QStringLiteral("↓ %1%").arg(percent);
            trend = QStringLiteral("down");
        } else {
            text = QStringLiteral("0.00%");
        }
    }
    QLabel *label = ui->totalExpensesChange;
    label->setText(text);
    label->setToolTip(QStringLiteral("Contra el mes anterior ($%1)")
                          .arg(locale.toString(summary.previousTotal, 'f', 2)));
    if (label->property("trend").toString() != trend) {
        label->setProperty("trend", trend);
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
}

application::OperationalExpenseFilterDto FinanceView::filter() const
{
    application::OperationalExpenseFilterDto result;
    const QVariant month = ui->periodCombo->currentData();
    if (month.isValid())
        result.month = month.toInt();
    const QVariant year = ui->yearCombo->currentData();
    if (year.isValid())
        result.year = year.toInt();
    return result;
}

void FinanceView::showWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(m_form ? static_cast<QWidget *>(m_form) : this, title, message);
}

void FinanceView::editInvoiceNumber(int folio)
{
    const int row = rowOf(folio);
    if (row < 0)
        return;
    QTableWidgetItem *number = ui->expensesTable->item(row, InvoiceNumberColumn);
    ui->expensesTable->setCurrentItem(number);
    ui->expensesTable->editItem(number);
}

bool FinanceView::confirmInvoiceUpdate()
{
    return confirm(QStringLiteral("Actualizar factura"),
                   QStringLiteral("¿Está seguro de realizar la actualización correspondiente a la "
                                  "Factura?"),
                   QStringLiteral("Este cambio puede afectar permanentemente el valor de "
                                  "No. Factura."));
}

bool FinanceView::confirmInvoiceNumberUpdate()
{
    return confirm(QStringLiteral("Actualizar No. Factura"),
                   QStringLiteral("¿Desea realizar la actualización del valor de No. Factura?"),
                   QStringLiteral("El número reemplazado no podrá recuperarse."));
}

int FinanceView::selectedExpenseFolio()
{
    const QList<QTableWidgetItem *> selected = ui->expensesTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    const QTableWidgetItem *folio = ui->expensesTable->item(selected.first()->row(), FolioColumn);
    return folio ? folio->data(kFolioRole).toInt() : -1;
}

QString FinanceView::askReceiptPath(const QString &suggestedName)
{
    return QFileDialog::getSaveFileName(this, QStringLiteral("Guardar recibo"), suggestedName,
                                        QStringLiteral("PDF (*.pdf)"));
}

void FinanceView::openDocument(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool FinanceView::askForMissingInvoiceNumber()
{
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Falta el No. Factura"),
                    QStringLiteral("Ingresa el valor de No. Factura."), QMessageBox::NoButton, this);
    box.setInformativeText(QStringLiteral("La factura no se guarda sin su número. Si sales sin "
                                          "ingresarlo, la casilla Factura se desmarca."));
    QPushButton *capture =
        box.addButton(QStringLiteral("Ingresar No. Factura"), QMessageBox::AcceptRole);
    QPushButton *leave = box.addButton(QStringLiteral("Salir sin factura"), QMessageBox::RejectRole);
    box.setDefaultButton(capture);
    box.setEscapeButton(leave);
    box.exec();
    return box.clickedButton() == capture;
}

bool FinanceView::confirm(const QString &title, const QString &question, const QString &detail)
{
    QMessageBox box(QMessageBox::Question, title, question, QMessageBox::NoButton, this);
    box.setInformativeText(detail);
    // Con estos roles cada plataforma los acomoda en su orden habitual.
    QPushButton *cancel = box.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    QPushButton *accept = box.addButton(QStringLiteral("Aceptar"), QMessageBox::AcceptRole);
    // Cancelar de entrada: un Enter de más no debe borrar un número.
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == accept;
}

void FinanceView::askNewExpense(const QList<application::ExpenseTypeOptionDto> &expenseTypes)
{
    if (m_form) {
        m_form->raise();
        m_form->activateWindow();
        return;
    }
    m_form = new AddExpenseDialog(expenseTypes, window());
    m_form->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_form, &AddExpenseDialog::submitted, this, &FinanceView::expenseSubmitted);
    // open() y no exec(): modal igual, pero sin un bucle de eventos anidado
    // mientras el presenter guarda en otro hilo.
    m_form->open();
}

void FinanceView::setExpenseFormBusy(bool busy)
{
    if (m_form)
        m_form->setBusy(busy);
}

void FinanceView::showExpenseFormErrors(const QList<domain::ValidationError> &errors)
{
    if (m_form)
        m_form->showErrors(errors);
}

void FinanceView::closeExpenseForm()
{
    if (m_form)
        m_form->accept();
}

int FinanceView::rowOf(int folio) const
{
    for (int row = 0; row < ui->expensesTable->rowCount(); ++row) {
        const QTableWidgetItem *folioCell = ui->expensesTable->item(row, FolioColumn);
        if (folioCell && folioCell->data(kFolioRole).toInt() == folio)
            return row;
    }
    return -1;
}

void FinanceView::syncInvoiceNumberEditable(int row)
{
    const QTableWidgetItem *invoice = ui->expensesTable->item(row, InvoiceColumn);
    QTableWidgetItem *number = ui->expensesTable->item(row, InvoiceNumberColumn);
    if (!invoice || !number)
        return;
    const QScopedValueRollback<bool> painting(m_painting, true);
    if (invoice->checkState() == Qt::Checked) {
        number->setFlags(number->flags() | Qt::ItemIsEditable);
        // Recién marcada: "Sin Factura" no es un número que editar.
        if (number->data(kPlaceholderRole).toBool()) {
            number->setText(QString());
            number->setData(kPlaceholderRole, QVariant());
            number->setData(Qt::ForegroundRole, QVariant());
        }
    } else {
        number->setFlags(number->flags() & ~Qt::ItemIsEditable);
    }
}
