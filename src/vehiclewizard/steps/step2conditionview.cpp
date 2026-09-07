#include "../../../include/vehiclewizard/steps/step2conditionview.h"
#include "../../../include/db/connectionpool.h"
#include "../../../include/vehiclewizard/components/conditionchecklistitems.h"
#include "../../../include/vehiclewizard/components/conditionchecklistrow.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVBoxLayout>

Step2ConditionView::Step2ConditionView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->addWidget(buildBasicSpecsPanel(), 0);
    layout->addWidget(buildChecklistPanel(), 1);
}

QWidget *Step2ConditionView::buildBasicSpecsPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));

    auto *title = new QLabel(QStringLiteral("Especificaciones básicas"), card);
    title->setProperty("class", QStringLiteral("h3"));

    m_fuelTypeCombo = new QComboBox(card);
    m_cylindersCombo = new QComboBox(card);
    m_cylindersCombo->addItems({QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5"),
                                 QStringLiteral("6"), QStringLiteral("8")});
    m_transmissionCombo = new QComboBox(card);
    m_transmissionCombo->addItems({QStringLiteral("Automático"), QStringLiteral("Manual")});
    m_interiorMaterialCombo = new QComboBox(card);
    m_interiorMaterialCombo->setEditable(true);
    m_interiorMaterialCombo->addItems({QStringLiteral("Tela"), QStringLiteral("Piel"), QStringLiteral("Piel sintética")});
    m_windowRegulatorsCombo = new QComboBox(card);
    m_windowRegulatorsCombo->addItems({QStringLiteral("Manuales"), QStringLiteral("Eléctricos tradicionales"),
                                        QStringLiteral("Eléctricos inteligentes")});
    m_airConditioningCombo = new QComboBox(card);
    m_airConditioningCombo->addItems({QStringLiteral("Automático"), QStringLiteral("Manual")});

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Combustible"), m_fuelTypeCombo);
    form->addRow(QStringLiteral("Cilindros"), m_cylindersCombo);
    form->addRow(QStringLiteral("Transmisión"), m_transmissionCombo);
    form->addRow(QStringLiteral("Interiores"), m_interiorMaterialCombo);
    form->addRow(QStringLiteral("Cristales"), m_windowRegulatorsCombo);
    form->addRow(QStringLiteral("Aire Acondicionado"), m_airConditioningCombo);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->addWidget(title);
    cardLayout->addLayout(form);
    cardLayout->addStretch();

    return card;
}

QWidget *Step2ConditionView::buildChecklistPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    auto *cardLayout = new QVBoxLayout(card);

    auto *title = new QLabel(QStringLiteral("Detalles de la condición del vehículo"), card);
    title->setProperty("class", QStringLiteral("h3"));
    cardLayout->addWidget(title);

    auto *scrollArea = new QScrollArea(card);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollContent = new QWidget(scrollArea);
    auto *scrollLayout = new QVBoxLayout(scrollContent);

    const QList<ConditionChecklistItemDef> &items = conditionChecklistItems();
    for (const QString &group : conditionChecklistGroups()) {
        auto *groupHeaderLayout = new QHBoxLayout;
        auto *groupLabel = new QLabel(group, scrollContent);
        groupLabel->setProperty("class", QStringLiteral("form-label"));
        auto *markAllButton = new QPushButton(QStringLiteral("Marcar todo"), scrollContent);
        markAllButton->setProperty("class", QStringLiteral("secondary"));
        connect(markAllButton, &QPushButton::clicked, this, [this, group]() { markAllInGroup(group, true); });

        groupHeaderLayout->addWidget(groupLabel);
        groupHeaderLayout->addStretch();
        groupHeaderLayout->addWidget(markAllButton);
        scrollLayout->addLayout(groupHeaderLayout);

        for (const ConditionChecklistItemDef &def : items) {
            if (def.group != group)
                continue;
            auto *row = new ConditionChecklistRow(def, scrollContent);
            scrollLayout->addWidget(row);
            m_checklistRows << row;
        }

        auto *separator = new QFrame(scrollContent);
        separator->setFrameShape(QFrame::HLine);
        scrollLayout->addWidget(separator);
    }
    scrollLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    cardLayout->addWidget(scrollArea, 1);

    return card;
}

void Step2ConditionView::markAllInGroup(const QString &group, bool checked)
{
    for (ConditionChecklistRow *row : std::as_const(m_checklistRows)) {
        if (row->value().itemGroup == group)
            row->setChecked(checked);
    }
}

void Step2ConditionView::loadLookups()
{
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();
    if (!db.isOpen())
        return;

    m_fuelTypeCombo->clear();
    QSqlQuery query(db);
    if (query.exec(QStringLiteral("SELECT id, name FROM fuel_type_cat ORDER BY name"))) {
        while (query.next())
            m_fuelTypeCombo->addItem(query.value(1).toString(), query.value(0));
    }
}

void Step2ConditionView::fillDraft(VehicleDraft &draft) const
{
    draft.fuelTypeId = m_fuelTypeCombo->currentData().toString();
    draft.cylinders = m_cylindersCombo->currentText().toInt();
    draft.transmission = m_transmissionCombo->currentText();
    draft.interiorMaterial = m_interiorMaterialCombo->currentText();
    draft.windowRegulators = m_windowRegulatorsCombo->currentText();
    draft.airConditioning = m_airConditioningCombo->currentText();

    draft.conditionItems.clear();
    for (ConditionChecklistRow *row : m_checklistRows)
        draft.conditionItems << row->value();
}
