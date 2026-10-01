#include "../../../include/vehiclewizard/components/conditionchecklistrow.h"

#include <QApplication>
#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QStyle>

#include <algorithm>

namespace {

// Espejo de resources/styles/global-style.qss: el font-size base de QWidget
// (12pt), el ancho del indicador de checkbox/radio (14px) y su spacing (8px).
// Están duplicados aquí porque la medición ocurre antes de que la hoja de
// estilos se aplique -- ver measureColumns().
constexpr int kStyleSheetBaseFontPointSize = 12;
constexpr int kIndicatorWidth = 14;
constexpr int kIndicatorSpacing = 8;
constexpr int kColumnPadding = 18;

} // namespace

ConditionChecklistRow::ColumnWidths
ConditionChecklistRow::measureColumns(const QList<ConditionCatalogItem> &items)
{
    // OJO: esto corre antes de que los widgets existan y se "pulan" con la
    // hoja de estilos, así que QWidget::font() todavía devuelve la fuente por
    // omisión de Windows, más chica que la del QSS. Medir con ella dejaba el
    // ancho corto y recortaba el texto. Por eso se fuerza el mismo tamaño que
    // declara la regla base del QSS.
    QFont measureFont = QApplication::font();
    measureFont.setPointSize(kStyleSheetBaseFontPointSize);
    const QFontMetrics metrics(measureFont);

    ColumnWidths widths;
    for (const ConditionCatalogItem &item : items) {
        widths.checkBox = std::max(widths.checkBox, metrics.horizontalAdvance(item.element));
        widths.faultyRadio = std::max(widths.faultyRadio,
                                      metrics.horizontalAdvance(item.negativeLabel));
    }

    const int chrome = kIndicatorWidth + kIndicatorSpacing + kColumnPadding;
    widths.checkBox += chrome;
    widths.faultyRadio += chrome;
    return widths;
}

ConditionChecklistRow::ConditionChecklistRow(const ConditionCatalogItem &item,
                                             const ColumnWidths &columns, QWidget *parent)
    : QWidget(parent)
    , m_item(item)
{
    // Este widget contenedor es lo que engancha la regla del QSS que agrisa
    // la fila cuando se desmarca: el selector es de descendiente y necesita un
    // contenedor por fila que cargue la propiedad itemChecked. Subir los hijos
    // al layout del padre rompería el estilo en silencio.
    setProperty("class", QStringLiteral("condition-row"));

    m_checkBox = new QCheckBox(item.element, this);
    m_checkBox->setChecked(true);
    m_checkBox->setFixedWidth(columns.checkBox);

    m_optimalRadio = new QRadioButton(QStringLiteral("Estado óptimo"), this);
    m_optimalRadio->setChecked(true);
    m_faultyRadio = new QRadioButton(QStringLiteral("Con fallos"), this);
    //m_faultyRadio->setFixedWidth(columns.faultyRadio);

    m_observationsEdit = new QLineEdit(this);
    m_observationsEdit->setPlaceholderText(QStringLiteral("Observaciones"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->addWidget(m_checkBox);
    layout->addWidget(m_optimalRadio);
    layout->addSpacing(16); // aire extra entre "Estado óptimo" y el siguiente radio
    layout->addWidget(m_faultyRadio);
    layout->addSpacing(16);
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

bool ConditionChecklistRow::isChecked() const
{
    return m_checkBox->isChecked();
}

const QString &ConditionChecklistRow::category() const
{
    return m_item.category;
}

std::optional<domain::InspectionItem> ConditionChecklistRow::value() const
{
    if (!m_checkBox->isChecked())
        return std::nullopt;

    // La etiqueta del radio negativo varía por ítem ("Deteriorada",
    // "Gastadas"), pero lo que se guarda es un booleano: el texto es solo
    // presentación.
    return domain::InspectionItem(m_item.id, m_optimalRadio->isChecked(),
                                  m_observationsEdit->text());
}
