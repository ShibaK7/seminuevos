//
// Created by Alucard on 14/10/2026.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CommercialMainView.h" resolved

#include "commercialmainview.h"
#include "ui_CommercialMainView.h"

#include "presentation/commercial/commercialpresenter.h"

#include <QComboBox>
#include <QDate>
#include <QHeaderView>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QFont>

namespace {

enum SaleColumn {
    ColFolio = 0,
    ColSaleDate,
    ColBuyer,
    ColVehicle,
    ColPaymentType,
    ColTotalAmount,
    ColProfit,
    ColStatus,
};

enum CollectionColumn {
    CollectionFolio = 0,
    CollectionSaleDate,
    CollectionBuyer,
    CollectionVehicle,
    CollectionTotal,
    CollectionPaid,
    CollectionBalance,
};

constexpr auto kProfitPlaceholder = "TBD";

class SortableItem final : public QTableWidgetItem
{
public:
    explicit SortableItem(const QString &text)
        : QTableWidgetItem(text)
    {
    }

    bool operator<(const QTableWidgetItem &other) const override
    {
        const QVariant left = data(Qt::UserRole);
        const QVariant right = other.data(Qt::UserRole);
        if (left.isValid() && right.isValid()) {
            if (left.metaType().id() == QMetaType::QDate)
                return left.toDate() < right.toDate();
            return left.toDouble() < right.toDouble();
        }
        return QTableWidgetItem::operator<(other);
    }
};

QString money(double amount)
{
    return QLocale(QLocale::Spanish, QLocale::Mexico)
        .toCurrencyString(amount, QStringLiteral("$"), 2);
}

} // namespace

CommercialMainView::CommercialMainView(QWidget* parent) :
    QWidget(parent), ui(new Ui::CommercialMainView)
{
    ui->setupUi(this);
    ui->salesTab->click();
    ui->newSaleButton->setIcon(QIcon(":/icons/plus.png"));
    ui->newSaleButton->setIconSize(QSize(20,20));
    ui->newSaleButton->show();
    connect(ui->newSaleButton, &QPushButton::clicked, this, &CommercialMainView::newSaleRequested);
    ui->searchBox->hide();

    /*QPalette palete = ui->tableWidget->palette();
    palete.setColor(QPalette::Base, QColor("#f9f9f9"));          // Fila normal
    palete.setColor(QPalette::AlternateBase, QColor("#e0f2fe")); // Fila alterna
    palete.setColor(QPalette::Highlight, QColor("#3b82f6"));     // Fondo seleccionado
    palete.setColor(QPalette::HighlightedText, QColor("#ffffff")); // Texto seleccionado
    ui->tableWidget->setPalette(palete);*/
    ui->tableWidget->setProperty("class", QStringLiteral("data-table"));
    ui->tableWidget->setSortingEnabled(true);
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setShowGrid(false);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(36);
    ui->tableWidget->horizontalHeader()->setMinimumSectionSize(72);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(false);

    ui->ComboMonth->insertItem(0, QStringLiteral("Todos"));
    ui->ComboMonth->setCurrentIndex(0);

    ui->ComboYear->clear();
    ui->ComboYear->addItem(QStringLiteral("Todos"));
    const int currentYear = QDate::currentDate().year();
    for (int year = currentYear; year >= currentYear - 15; --year)
        ui->ComboYear->addItem(QString::number(year), year);

    ui->ComboPaymentType->clear();
    ui->ComboPaymentType->addItem(QStringLiteral("Todos"), QString());
    ui->ComboPaymentType->addItem(QStringLiteral("Contado"), QStringLiteral("Contado"));
    ui->ComboPaymentType->addItem(QStringLiteral("Crédito"), QStringLiteral("Crédito"));

    setCollectionsMode(false);
    m_searchTimer.setSingleShot(true);
    m_searchTimer.setInterval(250);
}

CommercialMainView::~CommercialMainView()
{
    delete ui;
}

void CommercialMainView::bind(presentation::CommercialPresenter &presenter)
{
    connect(ui->ComboMonth, &QComboBox::currentIndexChanged, &presenter,
            &presentation::CommercialPresenter::reload);
    connect(ui->ComboYear, &QComboBox::currentIndexChanged, &presenter,
            &presentation::CommercialPresenter::reload);
    connect(ui->ComboPaymentType, &QComboBox::currentIndexChanged, &presenter,
            &presentation::CommercialPresenter::reload);
    connect(ui->searchBox, &QLineEdit::textChanged, &m_searchTimer,
            qOverload<>(&QTimer::start));
    connect(&m_searchTimer, &QTimer::timeout, &presenter,
            &presentation::CommercialPresenter::reload);
    connect(ui->salesTab, &QPushButton::clicked, this, [&presenter, this] {
        setCollectionsMode(false, &presenter);
    });
    connect(ui->collectionsTab, &QPushButton::clicked, this, [&presenter, this] {
        setCollectionsMode(true, &presenter);
    });
    connect(ui->tableWidget, &QTableWidget::cellClicked, this,
            [this](int row, int column) {
                if (column != 0)
                    return;
                const QTableWidgetItem *item = ui->tableWidget->item(row, column);
                if (!item)
                    return;
                const int folio = item->data(Qt::UserRole).toInt();
                if (m_collectionsMode) {
                    emit saleDetailsRequested(folio);
                } else {
                    emit fullSaleDetailsRequested(folio);
                }
            });
}

application::CommercialFilterDto CommercialMainView::filter() const
{
    application::CommercialFilterDto selected;
    if (ui->ComboMonth->currentIndex() > 0)
        selected.month = ui->ComboMonth->currentIndex();
    if (ui->ComboYear->currentIndex() > 0)
        selected.year = ui->ComboYear->currentData().toInt();
    if (!m_collectionsMode)
        selected.paymentType = ui->ComboPaymentType->currentData().toString();
    if (m_collectionsMode)
        selected.searchText = ui->searchBox->text();
    selected.collectionsMode = m_collectionsMode;
    return selected;
}

void CommercialMainView::showSales(const QList<application::CommercialSaleDto> &sales)
{
    ui->tableWidget->setSortingEnabled(false);
    ui->tableWidget->clearSpans();
    ui->tableWidget->clearContents();
    configureTableColumns();
    ui->tableWidget->setRowCount(sales.size());

    for (qsizetype row = 0; row < sales.size(); ++row) {
        const application::CommercialSaleDto &sale = sales.at(row);
        auto *folio = new SortableItem(QString::number(sale.folio));
        folio->setData(Qt::UserRole, sale.folio);
        folio->setForeground(QColor(QStringLiteral("#1769aa")));
        QFont folioFont = folio->font();
        folioFont.setUnderline(true);
        folio->setFont(folioFont);
        folio->setToolTip(QStringLiteral("Ver folio %1").arg(sale.folio));
        folio->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 0, folio);

        auto *date = new SortableItem(sale.saleDate.toString(QStringLiteral("dd/MM/yyyy")));
        date->setData(Qt::UserRole, sale.saleDate);
        date->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 1, date);
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(sale.buyerName));
        ui->tableWidget->setItem(
            row, 3,
            new QTableWidgetItem(QStringLiteral("%1/%2").arg(sale.model).arg(sale.yearModel)));

        if (m_collectionsMode) {
            auto *total = new SortableItem(money(sale.totalAmount));
            total->setData(Qt::UserRole, sale.totalAmount);
            auto *paid = new SortableItem(money(sale.paidAmount));
            paid->setData(Qt::UserRole, sale.paidAmount);
            auto *balance = new SortableItem(money(sale.balanceDue));
            balance->setData(Qt::UserRole, sale.balanceDue);
            for (QTableWidgetItem *item : {total, paid, balance})
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->tableWidget->setItem(row, CollectionTotal, total);
            ui->tableWidget->setItem(row, CollectionPaid, paid);
            ui->tableWidget->setItem(row, CollectionBalance, balance);
            continue;
        }

        ui->tableWidget->setItem(row, ColPaymentType, new QTableWidgetItem(sale.paymentType));
        auto *total = new SortableItem(money(sale.totalAmount));
        total->setData(Qt::UserRole, sale.totalAmount);
        total->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->tableWidget->setItem(row, ColTotalAmount, total);

        auto *profit = new SortableItem(QString::fromLatin1(kProfitPlaceholder));
        profit->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->tableWidget->setItem(row, ColProfit, profit);
        ui->tableWidget->setItem(row, ColStatus, new QTableWidgetItem(sale.status));
    }

    ui->tableWidget->setSortingEnabled(true);
}

void CommercialMainView::showCommercialMessage(const QString &message)
{
    ui->tableWidget->setSortingEnabled(false);
    ui->tableWidget->clearSpans();
    ui->tableWidget->clearContents();
    configureTableColumns();
    ui->tableWidget->setRowCount(1);
    auto *item = new QTableWidgetItem(message);
    item->setFlags(Qt::NoItemFlags);
    ui->tableWidget->setItem(0, 0, item);
    ui->tableWidget->setSpan(0, 0, 1, ui->tableWidget->columnCount());
    ui->tableWidget->setSortingEnabled(true);
}

void CommercialMainView::setCollectionsMode(bool enabled,
                                            presentation::CommercialPresenter *presenter)
{
    m_collectionsMode = enabled;
    if (!enabled)
        m_searchTimer.stop();
    ui->salesTab->setChecked(!enabled);
    ui->collectionsTab->setChecked(enabled);
    ui->newSaleButton->setVisible(!enabled);
    ui->searchBox->setVisible(enabled);
    ui->searchBox->setPlaceholderText(
        enabled ? QStringLiteral("Buscar comprador") : QStringLiteral("Buscar"));
    ui->label_3->setVisible(!enabled);
    ui->label_3->setEnabled(!enabled);
    ui->ComboPaymentType->setVisible(!enabled);
    ui->ComboPaymentType->setEnabled(!enabled);
    configureTableColumns();
    if (presenter)
        presenter->reload();
}

void CommercialMainView::configureTableColumns()
{
    auto *header = ui->tableWidget->horizontalHeader();
    ui->tableWidget->clearSpans();
    if (m_collectionsMode) {
        ui->tableWidget->setColumnCount(7);
        ui->tableWidget->setHorizontalHeaderLabels({
            QStringLiteral("Folio"), QStringLiteral("Fecha de Venta"),
            QStringLiteral("Comprador"), QStringLiteral("Vehículo (Modelo/Año)"),
            QStringLiteral("Total"), QStringLiteral("Pagos"), QStringLiteral("Adeudo")
        });
        header->setSectionResizeMode(QHeaderView::ResizeToContents);
        header->setSectionResizeMode(CollectionBuyer, QHeaderView::Stretch);
        header->setSectionResizeMode(CollectionVehicle, QHeaderView::ResizeToContents);
    } else {
        ui->tableWidget->setColumnCount(8);
        ui->tableWidget->setHorizontalHeaderLabels({
            QStringLiteral("Folio"), QStringLiteral("Fecha de Venta"),
            QStringLiteral("Comprador"), QStringLiteral("Vehículo (Modelo/Año)"),
            QStringLiteral("Tipo de Pago"), QStringLiteral("Monto Total"),
            QStringLiteral("Utilidad"), QStringLiteral("Estado")
        });
        header->setSectionResizeMode(QHeaderView::ResizeToContents);
        header->setSectionResizeMode(ColBuyer, QHeaderView::Stretch);
        header->setSectionResizeMode(ColVehicle, QHeaderView::ResizeToContents);
    }
}
