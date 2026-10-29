#include "saledetailsview.h"
#include "domain/common/value_objects/enums.h"
#include "presentation/common/forms/formsupport.h"
#include "ui_saledetailsview.h"

#include <QCompleter>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTextEdit>
#include <QtMath>

SaleDetailsView::SaleDetailsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SaleDetailsView)
{
    ui->setupUi(this);

    const auto setField = [](QWidget *widget, const QString &field) {
        widget->setProperty("field", field);
    };
    setField(ui->vehicleCombo, QStringLiteral("vehicle"));
    setField(ui->sellerBox, QStringLiteral("vendor"));
    setField(ui->buyerEdit, QStringLiteral("buyer.fullName"));
    setField(ui->residenceEdit, QStringLiteral("buyer.streetAddress"));
    setField(ui->suburbEdit, QStringLiteral("buyer.suburb"));
    setField(ui->localityEdit, QStringLiteral("buyer.locality"));
    setField(ui->stateEdit, QStringLiteral("buyer.state"));
    setField(ui->zipCodeEdit, QStringLiteral("buyer.postalCode"));
    setField(ui->phoneEdit, QStringLiteral("buyer.phone"));
    setField(ui->emailEdit, QStringLiteral("buyer.email"));
    setField(ui->identificationEdit, QStringLiteral("buyer.nationalId"));
    setField(ui->gNameEdit, QStringLiteral("guarantor.fullName"));
    setField(ui->gIdentificationEdit, QStringLiteral("guarantor.nationalId"));
    setField(ui->gResidenceEdit, QStringLiteral("guarantor.streetAddress"));
    setField(ui->gSuburbEdit, QStringLiteral("guarantor.suburb"));
    setField(ui->gLocalityEdit, QStringLiteral("guarantor.locality"));
    setField(ui->gStateEdit, QStringLiteral("guarantor.state"));
    setField(ui->gZipCodeEdit, QStringLiteral("guarantor.postalCode"));
    setField(ui->gTelephoneEdit, QStringLiteral("guarantor.phone"));
    setField(ui->gEmailEdit, QStringLiteral("guarantor.email"));
    formsupport::addFieldErrorLabels(this);

    for (QFrame *card : {ui->saleDetailsCard, ui->guarantorCard, ui->costCard, ui->financingCard, ui->paymentsCard}) {
        card->setObjectName(QStringLiteral("cardPanel"));
        formsupport::applyFloatingShadow(card);
    }

    ui->guarantorCard->setVisible(false);
    ui->financingCard->setVisible(false);
    ui->paymentsCard->setVisible(false);

    for (QDoubleSpinBox *spin : {ui->subtotalSpin, ui->ivaSpin, ui->totalSpin,
                                 ui->salePriceSpin, ui->depositSpin, ui->financingSpin,
                                 ui->finalPriceSpin}) {
        spin->setDecimals(2);
        spin->setMaximum(9999999999.99);
        spin->setLocale(QLocale(QLocale::Spanish, QLocale::Mexico));
    }
    ui->subtotalSpin->setReadOnly(true);
    ui->salePriceSpin->setReadOnly(true);
    ui->finalPriceSpin->setReadOnly(true);
    ui->ivaSpin->setReadOnly(true);
    ui->totalSpin->setReadOnly(true);
    ui->monthsSpin->setMinimum(0);
    ui->monthsSpin->setMaximum(600);
    ui->interestRateSpin->setDecimals(2);
    ui->interestRateSpin->setRange(0.0, 1000.0);
    ui->interestRateSpin->setValue(5.0);

    auto *projectionModel = new QStandardItemModel(this);
    projectionModel->setHorizontalHeaderLabels({QStringLiteral("Pago"),
                                                 QStringLiteral("Fecha"),
                                                 QStringLiteral("Cuota"),
                                                 QStringLiteral("Capital"),
                                                 QStringLiteral("Interés"),
                                                 QStringLiteral("Saldo")});
    ui->paymentsTable->setModel(projectionModel);
    ui->paymentsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->paymentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->dateEdit->setMaximumDate(QDate::currentDate());
    ui->dateEdit->setDate(QDate::currentDate());

    for (domain::PaymentType value : domain::allPaymentTypes())
        ui->paymentTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    for (domain::PaymentMethod value : domain::allPaymentMethods())
        ui->paymentMethodCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    ui->vehicleCombo->clear();
    ui->vehicleCombo->setEditable(true);
    ui->vehicleCombo->setInsertPolicy(QComboBox::NoInsert);
    if (ui->vehicleCombo->lineEdit()) {
        ui->vehicleCombo->lineEdit()->setPlaceholderText(QStringLiteral("Busca un vehículo por marca, modelo, placa o VIN"));
        ui->vehicleCombo->lineEdit()->installEventFilter(this);
    }
    ui->vehicleCombo->setCurrentIndex(-1);
    auto *vehicleModel = new QStringListModel(this);
    auto *vehicleCompleter = new QCompleter(vehicleModel, this);
    vehicleCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    vehicleCompleter->setFilterMode(Qt::MatchContains);
    vehicleCompleter->setCompletionMode(QCompleter::PopupCompletion);
    ui->vehicleCombo->setCompleter(vehicleCompleter);

    ui->sellerBox->setEditable(true);
    ui->sellerBox->setInsertPolicy(QComboBox::NoInsert);
    ui->sellerBox->setPlaceholderText(QStringLiteral("Busca un vendedor"));
    ui->sellerBox->installEventFilter(this);
    ui->sellerBox->lineEdit()->setPlaceholderText(QStringLiteral("Busca un vendedor"));
    auto *vendorModel = new QStringListModel(this);
    auto *vendorCompleter = new QCompleter(vendorModel, this);
    vendorCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    vendorCompleter->setFilterMode(Qt::MatchContains);
    vendorCompleter->setCompletionMode(QCompleter::PopupCompletion);
    ui->sellerBox->setCompleter(vendorCompleter);
    connect(ui->sellerBox->lineEdit(), &QLineEdit::textEdited, this,
            [this](const QString &text) {
                m_vendorId = -1;
                m_vendorDropdownRequested = false;
                emit vendorSearchRequested(text.trimmed());
            });
    connect(vendorCompleter, qOverload<const QString &>(&QCompleter::activated), this,
            [this](const QString &text) { selectVendor(text); });
    connect(ui->sellerBox, qOverload<int>(&QComboBox::activated), this,
            [this](int index) {
                if (index >= 0 && index < m_vendorMatches.size()) {
                    const auto &vendor = m_vendorMatches.at(index);
                    selectVendor(QStringLiteral("%1 (%2)").arg(vendor.displayName, vendor.username));
                }
            });
    connect(ui->vehicleCombo->lineEdit(), &QLineEdit::textEdited, this,
            [this](const QString &text) {
                resetVehicleSelection();
                if (text.trimmed().isEmpty()) {
                    ui->vehicleCombo->setCurrentIndex(-1);
                    emit vehicleSearchRequested(QString());
                    return;
                }
                emit vehicleSearchRequested(text.trimmed());
            });
    connect(vehicleCompleter, qOverload<const QString &>(&QCompleter::activated), this,
            [this](const QString &text) { selectVehicle(text); });
    connect(ui->vehicleCombo, qOverload<int>(&QComboBox::activated), this,
            [this](int index) {
                if (index >= 0 && index < m_vehicleMatches.size())
                    applyVehicle(m_vehicleMatches.at(index));
            });

    connect(ui->paymentTypeCombo, &QComboBox::currentIndexChanged, this,
            &SaleDetailsView::onPaymentTypeChanged);
    connect(ui->calculateProjectionButton, &QPushButton::clicked, this,
            &SaleDetailsView::calculatePaymentProjection);
    connect(ui->depositSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double deposit) {
                const QSignalBlocker blocker(ui->financingSpin);
                ui->financingSpin->setValue(qMax(0.0, ui->salePriceSpin->value() - deposit));
                invalidatePaymentProjection();
            });
    connect(ui->financingSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double financed) {
                const QSignalBlocker blocker(ui->depositSpin);
                ui->depositSpin->setValue(qMax(0.0, ui->salePriceSpin->value() - financed));
                invalidatePaymentProjection();
            });
    connect(ui->monthsSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this] { invalidatePaymentProjection(); });
    connect(ui->interestRateSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this] { invalidatePaymentProjection(); });
    connect(ui->dateEdit, &QDateEdit::dateChanged, this,
            [this] { invalidatePaymentProjection(); });

    const auto configureLookup = [this](QLineEdit *edit, bool guarantor,
                                         QList<QCompleter *> &completers) {
        auto *model = new QStringListModel(this);
        auto *completer = new QCompleter(model, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        edit->setCompleter(completer);
        completers << completer;
        connect(edit, &QLineEdit::textEdited, this, [this, guarantor](const QString &text) {
            if (guarantor)
                m_guarantorCounterpartyId = -1;
            else
                m_buyerCounterpartyId = -1;
            emit counterpartySearchRequested(text.trimmed(), guarantor);
        });
        connect(completer, qOverload<const QString &>(&QCompleter::activated), this,
                [this, guarantor](const QString &text) { selectCounterparty(text, guarantor); });
    };
    configureLookup(ui->buyerEdit, false, m_buyerCompleters);
    configureLookup(ui->identificationEdit, false, m_buyerCompleters);
    configureLookup(ui->gNameEdit, true, m_guarantorCompleters);
    configureLookup(ui->gIdentificationEdit, true, m_guarantorCompleters);
    //emit vehicleSearchRequested(QString());
}

SaleDetailsView::~SaleDetailsView()
{
    delete ui;
}

bool SaleDetailsView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->vehicleCombo->lineEdit()) {
        if (event->type() == QEvent::FocusIn) {
            if (ui->vehicleCombo->lineEdit()->text().trimmed().isEmpty())
                emit vehicleSearchRequested(QString());
        } else if (event->type() == QEvent::MouseButtonPress
                   && ui->vehicleCombo->lineEdit()->text().trimmed().isEmpty()
                   && !m_vehicleMatches.isEmpty()) {
            auto *completer = ui->vehicleCombo->completer();
            completer->setCompletionPrefix(QString());
            completer->complete();
        }
    }
    if (watched == ui->sellerBox && event->type() == QEvent::MouseButtonPress) {
        m_vendorDropdownRequested = true;
        emit vendorSearchRequested(QString());
    }
    if (watched == ui->sellerBox->lineEdit() && event->type() == QEvent::FocusIn
        && ui->sellerBox->lineEdit()->text().trimmed().isEmpty()) {
        m_vendorDropdownRequested = true;
        emit vendorSearchRequested(QString());
    }
    return QWidget::eventFilter(watched, event);
}

domain::PaymentType SaleDetailsView::selectedPaymentType() const
{
    return static_cast<domain::PaymentType>(ui->paymentTypeCombo->currentData().toInt());
}

void SaleDetailsView::onPaymentTypeChanged()
{
    const domain::PaymentType type = selectedPaymentType();
    const bool isCredit = type == domain::PaymentType::Credito;

    ui->guarantorCard->setVisible(isCredit);
    ui->financingCard->setVisible(isCredit);
    ui->paymentsCard->setVisible(isCredit);
    if (isCredit) {
        ui->rightFormBottomVerticalSpacer->changeSize(20, 0, QSizePolicy::Ignored, QSizePolicy::Minimum);
        if (m_vehicleFolio > 0 && !m_financingInitialized) {
           initializeFinancingDefaults();
        }
    } else {
        ui->rightFormBottomVerticalSpacer->changeSize(20, 40, QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    ui->rightFormsLayout->invalidate();
}

void SaleDetailsView::initializeFinancingDefaults()
{
    const double salePrice = ui->totalSpin->value();
    ui->salePriceSpin->setValue(salePrice);
    ui->depositSpin->setMaximum(salePrice);
    ui->financingSpin->setMaximum(salePrice);
    ui->depositSpin->setValue(qRound64(salePrice * 20.0) / 100.0);
    ui->financingSpin->setValue(salePrice - ui->depositSpin->value());
    ui->monthsSpin->setValue(12);
    ui->finalPriceSpin->setValue(0.0);
    ui->paymentsTable->model()->removeRows(0, ui->paymentsTable->model()->rowCount());
    m_projectionInstallments.clear();
    m_financingInitialized = true;
}

void SaleDetailsView::invalidatePaymentProjection()
{
    ui->finalPriceSpin->setValue(0.0);
    if (ui->paymentsTable->model())
        ui->paymentsTable->model()->removeRows(0, ui->paymentsTable->model()->rowCount());
    m_projectionInstallments.clear();
}

void SaleDetailsView::calculatePaymentProjection()
{
    const int months = ui->monthsSpin->value();
    const double principal = ui->financingSpin->value();
    const double monthlyRate = ui->interestRateSpin->value() / 1200.0;
    auto *model = qobject_cast<QStandardItemModel *>(ui->paymentsTable->model());
    model->removeRows(0, model->rowCount());
    m_projectionInstallments.clear();

    if (months <= 0 || principal <= 0.0) {
        ui->finalPriceSpin->setValue(0.0);
        ui->calculateProjectionButton->setToolTip(
            QStringLiteral("Captura un monto financiado y un plazo mayor que cero."));
        return;
    }

    ui->calculateProjectionButton->setToolTip(QString());
    const double regularPayment = monthlyRate == 0.0
        ? principal / months
        : principal * monthlyRate / (1.0 - qPow(1.0 + monthlyRate, -months));
    double balance = principal;
    double installmentTotal = 0.0;
    const QDate firstDueDate = ui->dateEdit->date().addMonths(1);
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);

    for (int month = 1; month <= months; ++month) {
        const double interest = qRound64(balance * monthlyRate * 100.0) / 100.0;
        const double capital = month == months
            ? balance
            : qMin(balance, qRound64((regularPayment - interest) * 100.0) / 100.0);
        const double payment = qRound64((capital + interest) * 100.0) / 100.0;
        balance = qMax(0.0, qRound64((balance - capital) * 100.0) / 100.0);
        installmentTotal += payment;

        const QList<QString> values = {
            QString::number(month),
            firstDueDate.addMonths(month - 1).toString(QStringLiteral("dd/MM/yyyy")),
            locale.toCurrencyString(payment),
            locale.toCurrencyString(capital),
            locale.toCurrencyString(interest),
            locale.toCurrencyString(balance)
        };
        QList<QStandardItem *> row;
        for (const QString &value : values)
            row << new QStandardItem(value);
        model->appendRow(row);

        application::SaleInstallmentDto installment;
        installment.number = month;
        installment.dueDate = firstDueDate.addMonths(month - 1);
        installment.expectedAmount = payment;
        installment.capitalAllocation = capital;
        installment.interestAllocation = interest;
        installment.remainingBalance = balance;
        m_projectionInstallments << installment;
    }

    ui->finalPriceSpin->setValue(ui->depositSpin->value() + installmentTotal);
}

void SaleDetailsView::setCounterpartyMatches(
    bool guarantor, const QList<application::CounterpartyDto> &matches)
{
    QList<QCompleter *> &completers = guarantor ? m_guarantorCompleters : m_buyerCompleters;
    if (guarantor)
        m_guarantorMatches = matches;
    else
        m_buyerMatches = matches;

    QStringList labels;
    for (const application::CounterpartyDto &person : matches)
        labels << QStringLiteral("%1 — %2").arg(person.fullName, person.nationalId);

    for (QCompleter *completer : completers) {
        auto *model = qobject_cast<QStringListModel *>(completer->model());
        if (!model) {
            model = new QStringListModel(completer);
            completer->setModel(model);
        }
        model->setStringList(labels);
        if (completer->widget()) {
            auto *edit = qobject_cast<QLineEdit *>(completer->widget());
            if (edit && edit->hasFocus()) {
                completer->setCompletionPrefix(edit->text());
                completer->complete();
            }
        }
    }
}

void SaleDetailsView::showCounterpartySearchError(bool guarantor, const QString &message)
{
    const QString tooltip = message.isEmpty()
                                ? QString()
                                : QStringLiteral("No se pudo buscar la persona: %1").arg(message);
    if (guarantor) {
        ui->gNameEdit->setToolTip(tooltip);
        ui->gIdentificationEdit->setToolTip(tooltip);
    } else {
        ui->buyerEdit->setToolTip(tooltip);
        ui->identificationEdit->setToolTip(tooltip);
    }
}

void SaleDetailsView::setVehicleMatches(const QList<application::SaleVehicleOptionDto> &matches)
{
    const QString query = ui->vehicleCombo->lineEdit()->text();
    m_vehicleMatches = matches;

    QStringList labels;
    for (const auto &vehicle : matches) {
        labels << QStringLiteral("%1 %2 %3 — Folio %4")
                      .arg(vehicle.brandName.isEmpty() ? QStringLiteral("Vehículo") : vehicle.brandName)
                      .arg(vehicle.model)
                      .arg(vehicle.yearModel > 0 ? QString::number(vehicle.yearModel) : QString())
                      .arg(vehicle.folio);
    }

    auto *model = qobject_cast<QStringListModel *>(ui->vehicleCombo->completer()->model());
    if (!model) {
        model = new QStringListModel(ui->vehicleCombo->completer());
        ui->vehicleCombo->completer()->setModel(model);
    }
    model->setStringList(labels);
    ui->vehicleCombo->clear();
    ui->vehicleCombo->addItems(labels);
    ui->vehicleCombo->setCurrentIndex(-1);
    ui->vehicleCombo->lineEdit()->setText(query);
    if (ui->vehicleCombo->lineEdit()->hasFocus() && !labels.isEmpty()) {
        auto *completer = ui->vehicleCombo->completer();
        completer->setCompletionPrefix(query);
        completer->complete();
    }
}

void SaleDetailsView::showVehicleSearchError(const QString &message)
{
    ui->vehicleCombo->setToolTip(message.isEmpty()
                                    ? QString()
                                    : QStringLiteral("No se pudo buscar el vehículo: %1").arg(message));
}

void SaleDetailsView::setVendorMatches(const QList<application::SaleVendorOptionDto> &matches)
{
    const QString query = ui->sellerBox->lineEdit()->text();
    const bool showAll = m_vendorDropdownRequested;
    m_vendorMatches = matches;
    QStringList labels;
    for (const auto &vendor : matches)
        labels << QStringLiteral("%1 (%2)").arg(vendor.displayName, vendor.username);

    auto *model = qobject_cast<QStringListModel *>(ui->sellerBox->completer()->model());
    if (!model) {
        model = new QStringListModel(ui->sellerBox->completer());
        ui->sellerBox->completer()->setModel(model);
    }
    model->setStringList(labels);
    ui->sellerBox->clear();
    for (qsizetype i = 0; i < matches.size(); ++i)
        ui->sellerBox->addItem(labels.at(i), matches.at(i).id);
    ui->sellerBox->setCurrentIndex(-1);
    ui->sellerBox->lineEdit()->setText(query);
    if (ui->sellerBox->lineEdit()->hasFocus() && !labels.isEmpty()) {
        auto *completer = ui->sellerBox->completer();
        completer->setCompletionPrefix(showAll ? QString() : query);
        completer->complete();
    }
    m_vendorDropdownRequested = false;
}

void SaleDetailsView::showVendorSearchError(const QString &message)
{
    ui->sellerBox->setToolTip(message.isEmpty()
                                  ? QString()
                                  : QStringLiteral("No se pudo buscar el vendedor: %1").arg(message));
}

QList<domain::ValidationError> SaleDetailsView::showSaleValidationErrors(
    const QList<domain::ValidationError> &errors)
{
    return formsupport::showFieldErrors(this, errors);
}

bool SaleDetailsView::focusSaleField(const QString &field)
{
    return formsupport::focusField(this, field);
}

void SaleDetailsView::setCurrentUser(const application::SessionDto &session)
{
    m_currentUser = session;
    m_hasCurrentUser = true;
    m_vendorId = session.userId;
    const QString display = QStringLiteral("%1 (%2)").arg(session.displayName, session.username);
    ui->sellerBox->clear();
    ui->sellerBox->addItem(display, session.userId);
    ui->sellerBox->setCurrentIndex(0);
    emit vendorSearchRequested(QString());
}

void SaleDetailsView::resetForm()
{
    m_buyerCounterpartyId = -1;
    m_guarantorCounterpartyId = -1;
    m_vehicleFolio = -1;
    m_vendorId = m_hasCurrentUser ? m_currentUser.userId : -1;
    m_vendorDropdownRequested = false;
    m_financingInitialized = false;
    m_projectionInstallments.clear();
    m_vehicleMatches.clear();
    m_vendorMatches.clear();
    m_buyerMatches.clear();
    m_guarantorMatches.clear();

    ui->vehicleCombo->clear();
    ui->vehicleCombo->setCurrentIndex(-1);
    ui->vehicleCombo->lineEdit()->setText(QString());
    ui->vehicleCombo->setToolTip(QString());

    ui->sellerBox->clear();
    if (m_hasCurrentUser) {
        const QString display = QStringLiteral("%1 (%2)").arg(m_currentUser.displayName,
                                                              m_currentUser.username);
        ui->sellerBox->addItem(display, m_currentUser.userId);
        ui->sellerBox->setCurrentIndex(0);
    }
    ui->sellerBox->setToolTip(QString());

    ui->buyerEdit->clear();
    ui->identificationEdit->clear();
    ui->residenceEdit->clear();
    ui->suburbEdit->clear();
    ui->localityEdit->clear();
    ui->stateEdit->clear();
    ui->zipCodeEdit->clear();
    ui->phoneEdit->clear();
    ui->emailEdit->clear();
    ui->updateBuyerRecordCheck->setChecked(false);

    ui->gNameEdit->clear();
    ui->gIdentificationEdit->clear();
    ui->gResidenceEdit->clear();
    ui->gSuburbEdit->clear();
    ui->gLocalityEdit->clear();
    ui->gStateEdit->clear();
    ui->gZipCodeEdit->clear();
    ui->gTelephoneEdit->clear();
    ui->gEmailEdit->clear();
    ui->updateGuarantorRecordCheck->setChecked(false);

    ui->guarantorCard->setVisible(false);
    ui->financingCard->setVisible(false);
    ui->paymentsCard->setVisible(false);
    ui->rightFormBottomVerticalSpacer->changeSize(20, 40, QSizePolicy::Ignored, QSizePolicy::Expanding);
    ui->rightFormsLayout->invalidate();

    ui->paymentTypeCombo->setCurrentIndex(0);
    ui->paymentMethodCombo->setCurrentIndex(-1);
    ui->observationsTextEdit->clear();
    ui->dateEdit->setDate(QDate::currentDate());
    ui->subtotalSpin->setValue(0.0);
    ui->ivaSpin->setValue(0.0);
    ui->totalSpin->setValue(0.0);
    ui->salePriceSpin->setValue(0.0);
    ui->depositSpin->setValue(0.0);
    ui->financingSpin->setValue(0.0);
    ui->monthsSpin->setValue(0);
    ui->interestRateSpin->setValue(5.0);
    ui->finalPriceSpin->setValue(0.0);
    ui->calculateProjectionButton->setToolTip(QString());
    if (ui->paymentsTable->model())
        ui->paymentsTable->model()->removeRows(0, ui->paymentsTable->model()->rowCount());

    emit vendorSearchRequested(QString());
    emit vehicleSearchRequested(QString());
}

void SaleDetailsView::showSavedSale(
    const application::CommercialSaleRegistrationDetailsDto &details)
{
    const application::SaleRegistrationDto &sale = details.registration;
    const application::SaleBuyerDto &buyer = sale.buyer;
    const QSignalBlocker paymentTypeBlocker(ui->paymentTypeCombo);

    m_financingInitialized = true;
    ui->folioEdit->setText(QString::number(details.folio));
    ui->dateEdit->setDate(sale.saleDate);
    m_vehicleFolio = details.vehicle.folio;
    m_selectedVehicle = details.vehicle;
    ui->vehicleCombo->clear();
    const QString vehicleLabel = QStringLiteral("%1 %2 %3 — Folio %4")
                                     .arg(details.vehicle.brandName.isEmpty()
                                              ? QStringLiteral("Vehículo")
                                              : details.vehicle.brandName)
                                     .arg(details.vehicle.model)
                                     .arg(details.vehicle.yearModel > 0
                                              ? QString::number(details.vehicle.yearModel)
                                              : QString())
                                     .arg(details.vehicle.folio);
    ui->vehicleCombo->addItem(vehicleLabel);
    ui->vehicleCombo->setCurrentIndex(0);

    ui->sellerBox->clear();
    const QString vendorLabel = details.vendor.username.isEmpty()
                                    ? details.vendor.displayName
                                    : QStringLiteral("%1 (%2)")
                                          .arg(details.vendor.displayName, details.vendor.username);
    ui->sellerBox->addItem(vendorLabel, details.vendor.id);
    ui->sellerBox->setCurrentIndex(0);
    m_vendorId = details.vendor.id;

    m_buyerCounterpartyId = buyer.counterpartyId;
    ui->buyerEdit->setText(buyer.fullName);
    ui->identificationEdit->setText(buyer.nationalId);
    ui->residenceEdit->setText(buyer.streetAddress);
    ui->suburbEdit->setText(buyer.suburb);
    ui->localityEdit->setText(buyer.locality);
    ui->stateEdit->setText(buyer.state);
    ui->zipCodeEdit->setText(buyer.postalCode);
    ui->phoneEdit->setText(buyer.phone);
    ui->emailEdit->setText(buyer.email);
    ui->updateBuyerRecordCheck->setChecked(false);
    ui->updateBuyerRecordCheck->hide();

    ui->paymentTypeCombo->setCurrentIndex(
        ui->paymentTypeCombo->findData(static_cast<int>(sale.paymentType)));
    onPaymentTypeChanged();
    ui->paymentMethodCombo->setCurrentIndex(-1);
    if (sale.paymentMethod) {
        ui->paymentMethodCombo->setCurrentIndex(
            ui->paymentMethodCombo->findData(static_cast<int>(*sale.paymentMethod)));
    } else if (sale.paymentType == domain::PaymentType::Credito) {
        ui->paymentMethodCombo->addItem(QStringLiteral("Financiamiento"));
        ui->paymentMethodCombo->setCurrentIndex(ui->paymentMethodCombo->count() - 1);
    }

    ui->subtotalSpin->setValue(sale.subtotal);
    ui->ivaSpin->setValue(sale.iva);
    ui->totalSpin->setValue(sale.totalAmount);
    ui->salePriceSpin->setValue(sale.totalAmount);
    ui->observationsTextEdit->setPlainText(sale.observations);
    ui->updateGuarantorRecordCheck->setChecked(false);
    ui->updateGuarantorRecordCheck->hide();
    m_projectionInstallments.clear();
    ui->paymentsTable->model()->removeRows(0, ui->paymentsTable->model()->rowCount());

    if (sale.financing) {
        const application::SaleFinancingDto &financing = *sale.financing;
        m_financingInitialized = true;
        ui->depositSpin->setValue(financing.downPayment);
        ui->financingSpin->setValue(financing.amountToFinance);
        ui->interestRateSpin->setValue(financing.interestRatePercent);
        ui->monthsSpin->setValue(financing.monthsTerm);
        ui->finalPriceSpin->setValue(financing.finalPrice);
        if (financing.guarantor) {
            const application::SaleBuyerDto &guarantor = *financing.guarantor;
            m_guarantorCounterpartyId = guarantor.counterpartyId;
            ui->gNameEdit->setText(guarantor.fullName);
            ui->gIdentificationEdit->setText(guarantor.nationalId);
            ui->gResidenceEdit->setText(guarantor.streetAddress);
            ui->gSuburbEdit->setText(guarantor.suburb);
            ui->gLocalityEdit->setText(guarantor.locality);
            ui->gStateEdit->setText(guarantor.state);
            ui->gZipCodeEdit->setText(guarantor.postalCode);
            ui->gTelephoneEdit->setText(guarantor.phone);
            ui->gEmailEdit->setText(guarantor.email);
        }

        auto *model = qobject_cast<QStandardItemModel *>(ui->paymentsTable->model());
        for (const application::SaleInstallmentDto &installment : financing.installments) {
            const QList<QString> values = {
                QString::number(installment.number),
                installment.dueDate.toString(QStringLiteral("dd/MM/yyyy")),
                QLocale(QLocale::Spanish, QLocale::Mexico)
                    .toCurrencyString(installment.expectedAmount),
                QLocale(QLocale::Spanish, QLocale::Mexico)
                    .toCurrencyString(installment.capitalAllocation),
                QLocale(QLocale::Spanish, QLocale::Mexico)
                    .toCurrencyString(installment.interestAllocation),
                QLocale(QLocale::Spanish, QLocale::Mexico)
                    .toCurrencyString(installment.remainingBalance)
            };
            QList<QStandardItem *> row;
            for (const QString &value : values) {
                auto *item = new QStandardItem(value);
                item->setEditable(false);
                row << item;
            }
            model->appendRow(row);
            m_projectionInstallments << installment;
        }
    } else {
        m_financingInitialized = false;
        ui->depositSpin->setValue(0.0);
        ui->financingSpin->setValue(0.0);
        ui->monthsSpin->setValue(0);
        ui->finalPriceSpin->setValue(0.0);
    }

    for (QLineEdit *edit : findChildren<QLineEdit *>())
        edit->setReadOnly(true);
    for (QTextEdit *edit : findChildren<QTextEdit *>())
        edit->setReadOnly(true);
    for (QAbstractSpinBox *spin : findChildren<QAbstractSpinBox *>())
        spin->setReadOnly(true);
    for (QComboBox *combo : findChildren<QComboBox *>())
        combo->setEnabled(false);
    for (QCheckBox *check : findChildren<QCheckBox *>())
        check->setEnabled(false);
    ui->calculateProjectionButton->hide();
    ui->printButton->hide();
    ui->paymentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void SaleDetailsView::selectVendor(const QString &displayText)
{
    for (const auto &vendor : m_vendorMatches) {
        const QString label = QStringLiteral("%1 (%2)").arg(vendor.displayName, vendor.username);
        if (label == displayText) {
            m_vendorId = vendor.id;
            ui->sellerBox->setCurrentText(label);
            return;
        }
    }
    if (ui->sellerBox->currentIndex() >= 0)
        m_vendorId = ui->sellerBox->currentData().toInt();
}

int SaleDetailsView::selectedVehicleFolio() const
{
    return m_vehicleFolio;
}

application::SaleVehicleOptionDto SaleDetailsView::selectedVehicle() const
{
    return m_selectedVehicle;
}

application::SaleRegistrationDto SaleDetailsView::registrationForm() const
{
    application::SaleRegistrationDto sale;
    sale.vehicleFolio = m_vehicleFolio;
    sale.vendorId = m_vendorId;
    sale.saleDate = ui->dateEdit->date();
    sale.paymentType = selectedPaymentType();
    sale.subtotal = ui->subtotalSpin->value();
    sale.iva = ui->ivaSpin->value();
    sale.totalAmount = ui->totalSpin->value();
    sale.observations = ui->observationsTextEdit->toPlainText().trimmed();
    sale.updateBuyerRecord = shouldUpdateCounterparty(false);
    sale.updateGuarantorRecord = shouldUpdateCounterparty(true);

    const application::CounterpartyDto buyer = counterpartyForm(false);
    sale.buyer.counterpartyId = buyer.id;
    sale.buyer.fullName = buyer.fullName;
    sale.buyer.nationalId = buyer.nationalId;
    sale.buyer.streetAddress = buyer.streetAddress;
    sale.buyer.suburb = buyer.suburb;
    sale.buyer.locality = buyer.locality;
    sale.buyer.state = buyer.state;
    sale.buyer.postalCode = buyer.postalCode;
    sale.buyer.phone = buyer.phone;
    sale.buyer.email = buyer.email;

    if (sale.paymentType == domain::PaymentType::Contado) {
        if (ui->paymentMethodCombo->currentIndex() >= 0)
            sale.paymentMethod = static_cast<domain::PaymentMethod>(ui->paymentMethodCombo->currentData().toInt());
    } else {
        application::SaleFinancingDto financing;
        financing.downPayment = ui->depositSpin->value();
        financing.amountToFinance = ui->financingSpin->value();
        financing.interestRatePercent = ui->interestRateSpin->value();
        financing.monthsTerm = ui->monthsSpin->value();
        financing.monthlyPayment = m_projectionInstallments.isEmpty()
            ? 0.0 : m_projectionInstallments.first().expectedAmount;
        financing.finalPrice = ui->finalPriceSpin->value();
        financing.installments = m_projectionInstallments;

        const application::CounterpartyDto guarantor = counterpartyForm(true);
        if (guarantor.id > 0 || !guarantor.fullName.trimmed().isEmpty()) {
            application::SaleBuyerDto guarantorDto;
            guarantorDto.counterpartyId = guarantor.id;
            guarantorDto.fullName = guarantor.fullName;
            guarantorDto.nationalId = guarantor.nationalId;
            guarantorDto.streetAddress = guarantor.streetAddress;
            guarantorDto.suburb = guarantor.suburb;
            guarantorDto.locality = guarantor.locality;
            guarantorDto.state = guarantor.state;
            guarantorDto.postalCode = guarantor.postalCode;
            guarantorDto.phone = guarantor.phone;
            guarantorDto.email = guarantor.email;
            financing.guarantor = guarantorDto;
        }
        sale.financing = financing;
    }
    return sale;
}

application::CounterpartyDto SaleDetailsView::counterpartyForm(bool guarantor) const
{
    application::CounterpartyDto person;
    person.id = guarantor ? m_guarantorCounterpartyId : m_buyerCounterpartyId;
    if (guarantor) {
        person.fullName = ui->gNameEdit->text();
        person.nationalId = ui->gIdentificationEdit->text();
        person.streetAddress = ui->gResidenceEdit->text();
        person.suburb = ui->gSuburbEdit->text();
        person.locality = ui->gLocalityEdit->text();
        person.state = ui->gStateEdit->text();
        person.postalCode = ui->gZipCodeEdit->text();
        person.phone = ui->gTelephoneEdit->text();
        person.email = ui->gEmailEdit->text();
    } else {
        person.fullName = ui->buyerEdit->text().section(" —", 0, 0);
        qDebug() << "Buyer Name: " << person.fullName;
        person.nationalId = ui->identificationEdit->text();
        person.streetAddress = ui->residenceEdit->text();
        person.suburb = ui->suburbEdit->text();
        person.locality = ui->localityEdit->text();
        person.state = ui->stateEdit->text();
        person.postalCode = ui->zipCodeEdit->text();
        person.phone = ui->phoneEdit->text();
        person.email = ui->emailEdit->text();
    }
    return person;
}

bool SaleDetailsView::shouldUpdateCounterparty(bool guarantor) const
{
    return guarantor ? ui->updateGuarantorRecordCheck->isChecked()
                     : ui->updateBuyerRecordCheck->isChecked();
}

void SaleDetailsView::applyCounterparty(const application::CounterpartyDto &person, bool guarantor)
{
    if (guarantor) {
        m_guarantorCounterpartyId = person.id;
        ui->gNameEdit->setText(person.fullName);
        ui->gIdentificationEdit->setText(person.nationalId);
        ui->gResidenceEdit->setText(person.streetAddress);
        ui->gSuburbEdit->setText(person.suburb);
        ui->gLocalityEdit->setText(person.locality);
        ui->gStateEdit->setText(person.state);
        ui->gZipCodeEdit->setText(person.postalCode);
        ui->gTelephoneEdit->setText(person.phone);
        ui->gEmailEdit->setText(person.email);
    } else {
        m_buyerCounterpartyId = person.id;
        ui->buyerEdit->setText(person.fullName);
        ui->identificationEdit->setText(person.nationalId);
        ui->residenceEdit->setText(person.streetAddress);
        ui->suburbEdit->setText(person.suburb);
        ui->localityEdit->setText(person.locality);
        ui->stateEdit->setText(person.state);
        ui->zipCodeEdit->setText(person.postalCode);
        ui->phoneEdit->setText(person.phone);
        ui->emailEdit->setText(person.email);
    }
    emit counterpartySelected(person.id, guarantor);
}

void SaleDetailsView::selectCounterparty(const QString &displayText, bool guarantor)
{
    const QList<application::CounterpartyDto> &matches = guarantor ? m_guarantorMatches : m_buyerMatches;
    for (const application::CounterpartyDto &person : matches) {
        if (QStringLiteral("%1 — %2").arg(person.fullName, person.nationalId) == displayText) {
            applyCounterparty(person, guarantor);
            return;
        }
    }
}

void SaleDetailsView::resetVehicleSelection()
{
    m_vehicleFolio = -1;
    m_financingInitialized = false;
    for (QDoubleSpinBox *spin : {ui->subtotalSpin, ui->ivaSpin, ui->totalSpin, ui->salePriceSpin,
                                 ui->depositSpin, ui->financingSpin, ui->finalPriceSpin})
        spin->setValue(0.0);
    ui->depositSpin->setMaximum(9999999999.99);
    ui->financingSpin->setMaximum(9999999999.99);
    invalidatePaymentProjection();
}

void SaleDetailsView::applyCostCalculations(double vehiclePrice, bool isFinancing)
{
    ui->subtotalSpin->setValue(vehiclePrice);

    const double subtotal = ui->subtotalSpin->value();
    const double iva = subtotal * 0.16;
    ui->ivaSpin->setValue(iva);
    ui->totalSpin->setValue(subtotal + ui->ivaSpin->value());
    ui->salePriceSpin->setValue(ui->totalSpin->value());
    if (!isFinancing)
        m_financingInitialized = false;
    if (isFinancing)
        initializeFinancingDefaults();
}

void SaleDetailsView::applyVehicle(const application::SaleVehicleOptionDto &vehicle)
{
    m_vehicleFolio = vehicle.folio;
    m_selectedVehicle = vehicle;
    ui->vehicleCombo->setCurrentText(QStringLiteral("%1 %2 %3 — Folio %4")
                                        .arg(vehicle.brandName.isEmpty() ? QStringLiteral("Vehículo") : vehicle.brandName)
                                        .arg(vehicle.model)
                                        .arg(vehicle.yearModel > 0 ? QString::number(vehicle.yearModel) : QString())
                                        .arg(vehicle.folio));
    applyCostCalculations(vehicle.salePrice, selectedPaymentType() == domain::PaymentType::Credito);
    emit vehicleSelected(vehicle.folio);
}

void SaleDetailsView::selectVehicle(const QString &displayText)
{
    for (const application::SaleVehicleOptionDto &vehicle : std::as_const(m_vehicleMatches)) {
        const QString label = QStringLiteral("%1 %2 %3 — Folio %4")
                                 .arg(vehicle.brandName.isEmpty() ? QStringLiteral("Vehículo") : vehicle.brandName)
                                 .arg(vehicle.model)
                                 .arg(vehicle.yearModel > 0 ? QString::number(vehicle.yearModel) : QString())
                                 .arg(vehicle.folio);
        if (label == displayText) {
            applyVehicle(vehicle);
            return;
        }
    }
}
