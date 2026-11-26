#include "presentation/settings/systemsettingswidget.h"
#include "ui_systemsettingswidget.h"

#include "presentation/common/forms/formsupport.h"
#include "presentation/settings/systemsettingspresenter.h"

#include <QLocale>
#include <QMessageBox>
#include <QPushButton>

#include <cmath>

using domain::SystemParameter;

SystemSettingsWidget::SystemSettingsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SystemSettingsWidget)
{
    ui->setupUi(this);
    formsupport::addFieldErrorLabels(ui->parametersCard);
    // La sombra no cabe en el .ui: Designer no la declara.
    formsupport::applyFloatingShadow(ui->parametersCard);

    connect(ui->editButton, &QPushButton::clicked, this, &SystemSettingsWidget::editRequested);
    connect(ui->saveButton, &QPushButton::clicked, this, &SystemSettingsWidget::saveRequested);
    connect(ui->cancelButton, &QPushButton::clicked, this, &SystemSettingsWidget::cancelRequested);
    formsupport::watchEdits(ui->parametersCard, this, [this] { emit edited(); });

    showMessage(QString());
    showNotice(QString());
    showMode(presentation::FormMode::View, false);
}

SystemSettingsWidget::~SystemSettingsWidget()
{
    delete ui;
}

void SystemSettingsWidget::bind(presentation::SystemSettingsPresenter &presenter)
{
    using presentation::SystemSettingsPresenter;
    connect(this, &SystemSettingsWidget::editRequested, &presenter, &SystemSettingsPresenter::onEdit);
    connect(this, &SystemSettingsWidget::saveRequested, &presenter, &SystemSettingsPresenter::onSave);
    connect(this, &SystemSettingsWidget::cancelRequested, &presenter, &SystemSettingsPresenter::onCancel);
    connect(this, &SystemSettingsWidget::edited, &presenter, &SystemSettingsPresenter::onEdited);
}

QAbstractSpinBox *SystemSettingsWidget::fieldOf(SystemParameter parameter) const
{
    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        return ui->umaSpin;
    case SystemParameter::CashLimitUmaMultiple:
        return ui->cashLimitSpin;
    }
    return nullptr;
}

QLabel *SystemSettingsWidget::updatedLabelOf(SystemParameter parameter) const
{
    switch (parameter) {
    case SystemParameter::UmaDailyValue:
        return ui->umaUpdated;
    case SystemParameter::CashLimitUmaMultiple:
        return ui->cashLimitUpdated;
    }
    return nullptr;
}

void SystemSettingsWidget::setBusy(bool busy)
{
    m_busy = busy;
    showMode(m_mode, m_canEdit);
}

void SystemSettingsWidget::showParameters(const QList<application::SystemParameterDto> &parameters)
{
    const QLocale mexico(QLocale::Spanish, QLocale::Mexico);
    for (const application::SystemParameterDto &dto : parameters) {
        // El que nunca se capturó muestra su valor por omisión, que es el que
        // la app está usando (la UMA no tiene: queda "Sin capturar").
        const std::optional<qint64> hundredths =
            dto.hundredths ? dto.hundredths : domain::defaultHundredths(dto.parameter);
        if (auto *spin = qobject_cast<QSpinBox *>(fieldOf(dto.parameter)))
            formsupport::showValue(spin, static_cast<int>(hundredths.value_or(0) / 100));
        else if (auto *money = qobject_cast<QDoubleSpinBox *>(fieldOf(dto.parameter)))
            formsupport::showValue(money, static_cast<double>(hundredths.value_or(0)) / 100.0);
        m_painted.insert(dto.parameter, {hundredths, fieldValue(dto.parameter)});

        // La base guarda la hora en UTC: de noche, ya sería el día siguiente.
        updatedLabelOf(dto.parameter)
            ->setText(dto.updatedAt.isValid()
                          ? QStringLiteral("Actualizado el %1")
                                .arg(mexico.toString(dto.updatedAt.toLocalTime().date(),
                                                     QStringLiteral("d 'de' MMMM 'de' yyyy")))
                          : QStringLiteral("Sin configurar"));
    }
}

QList<presentation::ISystemSettingsView::EditedValue> SystemSettingsWidget::editedValues() const
{
    QList<EditedValue> values;
    for (const SystemParameter parameter : domain::allSystemParameters()) {
        std::optional<qint64> hundredths = fieldValue(parameter);
        const auto painted = m_painted.constFind(parameter);
        if (painted != m_painted.cend() && painted->shown == hundredths)
            hundredths = painted->stored;
        values << EditedValue{parameter, hundredths};
    }
    return values;
}

std::optional<qint64> SystemSettingsWidget::fieldValue(SystemParameter parameter) const
{
    std::optional<qint64> hundredths;
    if (auto *spin = qobject_cast<QSpinBox *>(fieldOf(parameter))) {
        hundredths = qint64(spin->value()) * 100;
    } else if (auto *money = qobject_cast<QDoubleSpinBox *>(fieldOf(parameter))) {
        hundredths = std::llround(money->value() * 100.0);
        // En la UMA, cero es "Sin capturar".
        if (parameter == SystemParameter::UmaDailyValue && *hundredths == 0)
            hundredths.reset();
    }
    return hundredths;
}

void SystemSettingsWidget::showMode(presentation::FormMode mode, bool canEdit)
{
    m_mode = mode;
    m_canEdit = canEdit;
    const bool editing = mode == presentation::FormMode::Edit;
    // Mientras guarda, tampoco: lo que se tecleara se perdería al volver a
    // consulta.
    formsupport::setReadOnly(ui->parametersCard, !editing || m_busy);
    ui->editButton->setVisible(!editing);
    ui->editButton->setEnabled(canEdit && !m_busy);
    ui->saveButton->setVisible(editing);
    ui->cancelButton->setVisible(editing);
    ui->saveButton->setEnabled(!m_busy);
    ui->cancelButton->setEnabled(!m_busy);
}

void SystemSettingsWidget::showFieldErrors(const QList<domain::ValidationError> &errors)
{
    const QList<domain::ValidationError> unshown =
        formsupport::showFieldErrors(ui->parametersCard, errors);
    for (const domain::ValidationError &error : errors) {
        if (formsupport::focusField(ui->parametersCard, error.field))
            break;
    }
    if (!unshown.isEmpty())
        showMessage(unshown.first().message);
}

void SystemSettingsWidget::showMessage(const QString &message)
{
    ui->messageLabel->setText(message);
    ui->messageLabel->setVisible(!message.isEmpty());
}

void SystemSettingsWidget::showNotice(const QString &notice)
{
    ui->noticeLabel->setText(notice);
    ui->noticeLabel->setVisible(!notice.isEmpty());
}

bool SystemSettingsWidget::confirmDiscardChanges()
{
    return QMessageBox::question(this, QStringLiteral("Parámetros del sistema"),
                                 QStringLiteral("Hay cambios sin guardar en los parámetros. "
                                                "¿Descartarlos?"),
                                 QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel)
           == QMessageBox::Discard;
}
