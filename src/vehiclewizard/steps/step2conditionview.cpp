#include "../../../include/vehiclewizard/steps/step2conditionview.h"
#include "../../../include/db/connectionpool.h"
#include "../../../include/vehiclewizard/components/conditionchecklistrow.h"
#include "utils/UIUtils.h"

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

#include <optional>

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
    UIUtils::applyFloatingShadow(card);

    auto *title = new QLabel(QStringLiteral("Especificaciones básicas"), card);
    title->setProperty("class", QStringLiteral("h3"));
    title->setStyleSheet("margin-bottom: 15px;");

    m_fuelTypeCombo = new QComboBox(card);
    m_cylindersCombo = new QComboBox(card);
    m_cylindersCombo->setPlaceholderText("Seleccione una opción...");
    m_cylindersCombo->addItems({QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5"),
                                 QStringLiteral("6"), QStringLiteral("8")});

    // Los combos de valores cerrados se llenan desde el dominio, que es la
    // misma fuente que decide qué texto acepta la base. Antes eran literales
    // repetidos aquí, y cualquier retoque a una etiqueta rompía el CHECK.
    m_transmissionCombo = new QComboBox(card);
    m_transmissionCombo->setPlaceholderText("Seleccione una opción...");
    for (domain::Transmission value : domain::allTransmissions())
        m_transmissionCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    m_interiorMaterialCombo = new QComboBox(card);
    //m_interiorMaterialCombo->setEditable(true);
    m_interiorMaterialCombo->setPlaceholderText("Seleccione una opción...");
    m_interiorMaterialCombo->addItems({QStringLiteral("Tela"), QStringLiteral("Piel"), QStringLiteral("Piel sintética")});

    m_windowRegulatorsCombo = new QComboBox(card);
    m_windowRegulatorsCombo->setPlaceholderText("Seleccione una opción...");
    for (domain::WindowRegulators value : domain::allWindowRegulators())
        m_windowRegulatorsCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    m_airConditioningCombo = new QComboBox(card);
    m_airConditioningCombo->setPlaceholderText("Seleccione una opción...");
    for (domain::AirConditioning value : domain::allAirConditioningModes())
        m_airConditioningCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Combustible <span style='color: #D90429; font-weight: bold;'>*</span>"), m_fuelTypeCombo);
    form->addRow(QStringLiteral("Cilindros <span style='color: #D90429; font-weight: bold;'>*</span>"), m_cylindersCombo);
    form->addRow(QStringLiteral("Transmisión <span style='color: #D90429; font-weight: bold;'>*</span>"), m_transmissionCombo);
    form->addRow(QStringLiteral("Interiores <span style='color: #D90429; font-weight: bold;'>*</span>"), m_interiorMaterialCombo);
    form->addRow(QStringLiteral("Cristales <span style='color: #D90429; font-weight: bold;'>*</span>"), m_windowRegulatorsCombo);
    form->addRow(QStringLiteral("Aire Acondicionado <span style='color: #D90429; font-weight: bold;'>*</span>"), m_airConditioningCombo);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->addWidget(title);
    cardLayout->addLayout(form);
    cardLayout->addStretch();

    cardLayout->setContentsMargins(16, 16, 16, 16);

    return card;
}

QWidget *Step2ConditionView::buildChecklistPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    UIUtils::applyFloatingShadow(card);


    auto *cardLayout = new QVBoxLayout(card);

    cardLayout->setContentsMargins(16, 16, 16, 16);

    auto *title = new QLabel(QStringLiteral("Detalles de la condición del vehículo"), card);
    title->setProperty("class", QStringLiteral("h3"));
    cardLayout->addWidget(title);

    m_checklistMessageLabel = new QLabel(card);
    m_checklistMessageLabel->setProperty("class", QStringLiteral("error-text"));
    m_checklistMessageLabel->setWordWrap(true);
    m_checklistMessageLabel->setVisible(false);
    cardLayout->addWidget(m_checklistMessageLabel);

    auto *scrollArea = new QScrollArea(card);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    // El contenedor se crea vacío y se puebla en loadLookups(). OJO: el
    // addStretch() final NO va aquí -- tiene que quedar después de las filas,
    // o el espaciador las empuja al fondo del área desplazable.
    m_checklistContent = new QWidget(scrollArea);
    m_checklistLayout = new QVBoxLayout(m_checklistContent);

    scrollArea->setWidget(m_checklistContent);
    cardLayout->addWidget(scrollArea, 1);

    return card;
}

void Step2ConditionView::showChecklistMessage(const QString &message)
{
    m_checklistMessageLabel->setText(message);
    m_checklistMessageLabel->setVisible(true);
}

void Step2ConditionView::markAllInGroup(const QString &category, bool checked)
{
    bool allChecked = true;
    int categoryCount = 0;

    for (ConditionChecklistRow *row : std::as_const(m_checklistRows)) {
        if (row->category() == category) {
            categoryCount++;
            if (!row->isChecked()) {
                allChecked = false;
                break;
            }
        }
    }

    if (categoryCount == 0) return;

    bool targetState = !allChecked;

    for (ConditionChecklistRow *row : std::as_const(m_checklistRows)) {
        if (row->category() == category) {
            row->setChecked(targetState);
        }
    }
}

void Step2ConditionView::loadLookups()
{
    UIUtils::populateComboBox(m_fuelTypeCombo, "fuel_type_cat");

    QList<ConditionCatalogItem> catalog;
    QString catalogError;

    {
        ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            showChecklistMessage(QStringLiteral(
                "No se pudo conectar con la base de datos, así que el checklist de condición "
                "no está disponible."));
            return;
        }

        catalog = ConditionCatalog::load(db, &catalogError);
        // El handle se libera aquí, antes de construir un par de cientos de
        // widgets: no tiene sentido retener una conexión del pool durante eso.
    }

    if (catalog.isEmpty()) {
        showChecklistMessage(
            catalogError.isEmpty()
                ? QStringLiteral("El catálogo de condiciones (vehicle_conditions_cat) está "
                                 "vacío. Revisa que la aplicación haya podido sembrarlo al "
                                 "arrancar.")
                : QStringLiteral("No se pudo leer el catálogo de condiciones: %1").arg(catalogError));
        return;
    }

    populateChecklist(catalog);
}

void Step2ConditionView::populateChecklist(const QList<ConditionCatalogItem> &items)
{
    const ConditionChecklistRow::ColumnWidths columns =
        ConditionChecklistRow::measureColumns(items);

    // Un solo recorrido: el catálogo ya viene ordenado y con las categorías
    // contiguas, así que basta abrir un encabezado cada vez que cambia.
    QString currentCategory;
    for (const ConditionCatalogItem &item : items) {
        if (item.category != currentCategory) {
            if (!currentCategory.isEmpty()) {
                auto *separator = new QFrame(m_checklistContent);
                separator->setFrameShape(QFrame::HLine);
                m_checklistLayout->addWidget(separator);
            }
            currentCategory = item.category;

            auto *groupHeaderLayout = new QHBoxLayout;
            auto *groupLabel = new QLabel(currentCategory, m_checklistContent);
            groupLabel->setProperty("class", QStringLiteral("h4"));
            auto *markAllButton = new QPushButton(QStringLiteral("Marcar todo"), m_checklistContent);
            markAllButton->setProperty("class", QStringLiteral("secondary"));
            const QString category = currentCategory;
            connect(markAllButton, &QPushButton::clicked, this,
                    [this, category]() { markAllInGroup(category, true); });

            groupHeaderLayout->addWidget(groupLabel);
            groupHeaderLayout->addStretch();
            groupHeaderLayout->addWidget(markAllButton);
            m_checklistLayout->addLayout(groupHeaderLayout);
        }

        auto *row = new ConditionChecklistRow(item, columns, m_checklistContent);
        m_checklistLayout->addWidget(row);
        m_checklistRows << row;
    }

    auto *separator = new QFrame(m_checklistContent);
    separator->setFrameShape(QFrame::HLine);
    m_checklistLayout->addWidget(separator);
    m_checklistLayout->addStretch();
}

void Step2ConditionView::applyTo(domain::VehicleBuilder &builder) const
{
    domain::VehicleConditions conditions;

    domain::CatalogRef fuelType;
    const QVariant fuelData = m_fuelTypeCombo->currentData();
    if (fuelData.isValid() && !fuelData.isNull())
        fuelType.id = fuelData.toInt();
    fuelType.name = m_fuelTypeCombo->currentText();
    conditions.setFuelType(fuelType);

    // Un combo sin elegir (índice -1) no se vuelca: así el dominio lo reporta
    // como faltante. Leerlo de todos modos convertía el userData vacío en 0,
    // es decir, en el primer valor del enum, y ese dato inventado llegaba a la
    // base sin que nadie lo hubiera capturado.
    if (m_cylindersCombo->currentIndex() >= 0)
        (void)conditions.setCylinders(m_cylindersCombo->currentText().toInt());
    if (m_transmissionCombo->currentIndex() >= 0) {
        conditions.setTransmission(
            static_cast<domain::Transmission>(m_transmissionCombo->currentData().toInt()));
    }
    (void)conditions.setInteriorMaterial(m_interiorMaterialCombo->currentText());
    if (m_windowRegulatorsCombo->currentIndex() >= 0) {
        conditions.setWindowRegulators(
            static_cast<domain::WindowRegulators>(m_windowRegulatorsCombo->currentData().toInt()));
    }
    if (m_airConditioningCombo->currentIndex() >= 0) {
        conditions.setAirConditioning(
            static_cast<domain::AirConditioning>(m_airConditioningCombo->currentData().toInt()));
    }

    builder.setConditions(conditions);

    // Solo se vuelcan las filas marcadas. Las desmarcadas no generan renglón:
    // en vehicle_inspection la ausencia ya significa "la unidad no lo trae", y
    // el checklist completo se reconstruye después con el LEFT JOIN contra el
    // catálogo (VehicleInspectionQuery).
    domain::Inspection inspection;
    for (ConditionChecklistRow *row : m_checklistRows) {
        if (const std::optional<domain::InspectionItem> item = row->value())
            inspection.setItem(*item);
    }

    builder.setInspection(inspection);
}

domain::ValidationResult Step2ConditionView::validate() const
{
    // Mismo criterio que Step1DetailsView::validate(): un builder desechable
    // con lo capturado, y las reglas las pone el dominio. validateConditionData()
    // revisa solo las condiciones y el checklist, no los datos del Paso 1.
    domain::VehicleBuilder builder;
    applyTo(builder);
    return builder.validateConditionData();
}
