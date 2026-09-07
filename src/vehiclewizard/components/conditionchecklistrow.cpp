#include "../../../include/vehiclewizard/components/conditionchecklistrow.h"

#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QStyle>

#include <algorithm>

namespace {

// Ancho necesario para que el texto más largo entre todos los ítems del
// checklist quepa sin recortarse -- se usa como ancho fijo en TODAS las
// filas para que el checkbox (y el radio de "con fallas", cuya etiqueta
// también varía por ítem) queden alineados entre sí, en vez de que cada
// fila se ajuste a su propio texto. Se calcula una sola vez (cacheado).
int maxTextWidth(const QFontMetrics &metrics, const QList<QString> &candidates)
{
    int width = 0;
    for (const QString &text : candidates)
        width = std::max(width, metrics.horizontalAdvance(text));
    return width;
}

} // namespace

ConditionChecklistRow::ConditionChecklistRow(const ConditionChecklistItemDef &def, QWidget *parent)
    : QWidget(parent)
    , m_def(def)
{
    setProperty("class", QStringLiteral("condition-row"));

    m_checkBox = new QCheckBox(def.label, this);
    m_checkBox->setChecked(true);

    m_optimalRadio = new QRadioButton(QStringLiteral("Estado óptimo"), this);
    m_optimalRadio->setChecked(true);
    m_faultyRadio = new QRadioButton(def.negativeStateLabel, this);

    {
        // OJO: m_checkBox->fontMetrics() aquí mismo, recién construido,
        // todavía no está "polished" con la hoja de estilos de la app (el
        // font-size:12pt de QWidget en global-style.qss se aplica más
        // tarde) -- mide con la fuente default de Windows (más chica), lo
        // que dejaba el ancho corto y recortaba el texto. Se fuerza 12pt
        // explícito, igual que la regla base de global-style.qss.
        QFont measureFont = m_checkBox->font();
        measureFont.setPointSize(12);
        const QFontMetrics metrics(measureFont);

        static const int checkBoxLabelWidth = [&metrics] {
            QList<QString> labels;
            for (const ConditionChecklistItemDef &item : conditionChecklistItems())
                labels << item.label;
            // Indicador (14px, ver global-style.qss) + su spacing (8px) + margen.
            return maxTextWidth(metrics, labels) + 14 + 8 + 18;
        }();

        static const int faultyRadioLabelWidth = [&metrics] {
            QList<QString> labels;
            for (const ConditionChecklistItemDef &item : conditionChecklistItems())
                labels << item.negativeStateLabel;
            return maxTextWidth(metrics, labels) + 14 + 8 + 18;
        }();

        m_checkBox->setFixedWidth(checkBoxLabelWidth);
        m_faultyRadio->setFixedWidth(faultyRadioLabelWidth);
    }

    m_observationsEdit = new QLineEdit(this);
    m_observationsEdit->setPlaceholderText(QStringLiteral("Observaciones"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->addWidget(m_checkBox);
    layout->addWidget(m_optimalRadio);
    layout->addSpacing(16); // aire extra entre "Estado óptimo" y el siguiente radio
    layout->addWidget(m_faultyRadio);
    layout->addWidget(m_observationsEdit, 1);

    connect(m_checkBox, &QCheckBox::toggled, this, &ConditionChecklistRow::updateRowState);
    updateRowState();
}

void ConditionChecklistRow::updateRowState()
{
    const bool checked = m_checkBox->isChecked();
    m_optimalRadio->setEnabled(checked);
    m_faultyRadio->setEnabled(checked);
    m_observationsEdit->setEnabled(checked);

    setProperty("itemChecked", checked ? QStringLiteral("true") : QStringLiteral("false"));
    style()->unpolish(this);
    style()->polish(this);
}

void ConditionChecklistRow::setChecked(bool checked)
{
    m_checkBox->setChecked(checked);
}

ConditionItemValue ConditionChecklistRow::value() const
{
    ConditionItemValue value;
    value.itemKey = m_def.itemKey;
    value.itemGroup = m_def.group;
    value.isChecked = m_checkBox->isChecked();
    value.status = m_optimalRadio->isChecked() ? QStringLiteral("Estado óptimo") : QStringLiteral("Con fallas");
    value.observations = m_observationsEdit->text();
    return value;
}
