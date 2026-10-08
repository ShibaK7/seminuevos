#include "presentation/views/wizard/vehicleconditionsview.h"
#include "presentation/views/wizard/conditionchecklistrow.h"
#include "presentation/views/support/formsupport.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <optional>

VehicleConditionsView::VehicleConditionsView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->addWidget(buildBasicSpecsPanel(), 0);
    layout->addWidget(buildChecklistPanel(), 1);

    // Cualquier cambio se avisa; el presenter decide cuándo revalidar. Las
    // filas del checklist se vigilan al crearlas (setChecklist).
    formsupport::watchEdits(this, this, [this] { emit edited(); });
}

QWidget *VehicleConditionsView::buildBasicSpecsPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    formsupport::applyFloatingShadow(card);

    auto *title = new QLabel(QStringLiteral("Especificaciones básicas"), card);
    title->setProperty("class", QStringLiteral("h3"));

    // Cada combo lleva en "field" la clave con la que el dominio reporta sus
    // errores: así el asistente sabe cuál marcar en rojo y a cuál darle el
    // foco. Las condiciones llegan con el prefijo "conditions." porque así
    // las agrupa VehicleBuilder::validateConditionData().
    m_fuelTypeCombo = new QComboBox(card);
    m_fuelTypeCombo->setProperty("field", QStringLiteral("conditions.fuelType"));
    m_cylindersCombo = new QComboBox(card);
    m_cylindersCombo->setProperty("field", QStringLiteral("conditions.cylinders"));
    m_cylindersCombo->setPlaceholderText("Seleccione una opción...");
    m_cylindersCombo->addItems({QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5"),
                                 QStringLiteral("6"), QStringLiteral("8")});

    // Los combos de valores cerrados se llenan desde el dominio, que es la
    // misma fuente que decide qué texto acepta la base. Antes eran literales
    // repetidos aquí, y cualquier retoque a una etiqueta rompía el CHECK.
    m_transmissionCombo = new QComboBox(card);
    m_transmissionCombo->setProperty("field", QStringLiteral("conditions.transmission"));
    m_transmissionCombo->setPlaceholderText("Seleccione una opción...");
    for (domain::Transmission value : domain::allTransmissions())
        m_transmissionCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    m_interiorMaterialCombo = new QComboBox(card);
    m_interiorMaterialCombo->setProperty("field", QStringLiteral("conditions.interiorMaterial"));
    //m_interiorMaterialCombo->setEditable(true);
    m_interiorMaterialCombo->setPlaceholderText("Seleccione una opción...");
    m_interiorMaterialCombo->addItems({QStringLiteral("Tela"), QStringLiteral("Piel"), QStringLiteral("Piel sintética")});

    m_windowRegulatorsCombo = new QComboBox(card);
    m_windowRegulatorsCombo->setProperty("field", QStringLiteral("conditions.windowRegulators"));
    m_windowRegulatorsCombo->setPlaceholderText("Seleccione una opción...");
    for (domain::WindowRegulators value : domain::allWindowRegulators())
        m_windowRegulatorsCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    m_airConditioningCombo = new QComboBox(card);
    m_airConditioningCombo->setProperty("field", QStringLiteral("conditions.airConditioning"));
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
    // El aire bajo el título lo pone el layout y no una hoja de estilos propia
    // del título: esa hoja le gana a la global en ese widget y deja estilo
    // regado por el código, fuera del QSS. El layout no suma su espaciado
    // alrededor de un espacio fijo, así que la separación total queda igual
    // que antes: estos 15 px más el espaciado normal entre elementos.
    cardLayout->addSpacing(15);
    cardLayout->addLayout(form);
    cardLayout->addStretch();

    cardLayout->setContentsMargins(16, 16, 16, 16);

    return card;
}

QWidget *VehicleConditionsView::buildChecklistPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    formsupport::applyFloatingShadow(card);


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
    // La clave va en el checklist completo y no en cada fila: el dominio
    // reporta los renglones de la inspección por su posición entre los
    // marcados ("inspection.items[2]..."), no por fila de la pantalla, así que
    // ninguna fila tiene una clave fija que llevar.
    scrollArea->setProperty("field", QStringLiteral("inspection"));

    // El contenedor se crea vacío y se puebla en setChecklist(). OJO: el
    // addStretch() final NO va aquí -- tiene que quedar después de las filas,
    // o el espaciador las empuja al fondo del área desplazable.
    m_checklistContent = new QWidget(scrollArea);
    m_checklistLayout = new QVBoxLayout(m_checklistContent);

    scrollArea->setWidget(m_checklistContent);
    cardLayout->addWidget(scrollArea, 1);

    return card;
}

void VehicleConditionsView::showChecklistMessage(const QString &message)
{
    m_checklistMessageLabel->setText(message);
    m_checklistMessageLabel->setVisible(true);
}

void VehicleConditionsView::markAllInGroup(const QString &category, bool checked)
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

void VehicleConditionsView::setFuelTypes(const QList<application::CatalogOptionDto> &fuelTypes)
{
    formsupport::fillCombo(m_fuelTypeCombo, fuelTypes);
}

void VehicleConditionsView::showFieldErrors(const QList<domain::ValidationError> &errors)
{
    formsupport::showFieldErrors(this, errors);
}

bool VehicleConditionsView::focusField(const QString &field)
{
    return formsupport::focusField(this, field);
}

void VehicleConditionsView::setChecklist(const QList<application::ChecklistItemDto> &items)
{
    const ConditionChecklistRow::ColumnWidths columns =
        ConditionChecklistRow::measureColumns(items);

    // Un solo recorrido: el catálogo ya viene ordenado y con las categorías
    // contiguas, así que basta abrir un encabezado cada vez que cambia.
    QString currentCategory;
    for (const application::ChecklistItemDto &item : items) {
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

    // Las filas nacen después del constructor, así que se vigilan aquí.
    formsupport::watchEdits(m_checklistContent, this, [this] { emit edited(); });
}

application::VehicleConditionsDto VehicleConditionsView::conditions() const
{
    application::VehicleConditionsDto dto;

    const QVariant fuelData = m_fuelTypeCombo->currentData();
    if (m_fuelTypeCombo->currentIndex() >= 0 && fuelData.isValid() && !fuelData.isNull())
        dto.fuelType.id = fuelData.toInt();
    dto.fuelType.name = m_fuelTypeCombo->currentText();

    // Un combo sin elegir (índice -1) queda vacío: así el dominio lo reporta
    // como faltante. Leerlo de todos modos convertía el userData vacío en 0, es
    // decir, en el primer valor del enum.
    if (m_cylindersCombo->currentIndex() >= 0)
        dto.cylinders = m_cylindersCombo->currentText().toInt();
    if (m_transmissionCombo->currentIndex() >= 0)
        dto.transmission = static_cast<domain::Transmission>(m_transmissionCombo->currentData().toInt());
    if (m_interiorMaterialCombo->currentIndex() >= 0)
        dto.interiorMaterial = m_interiorMaterialCombo->currentText();
    if (m_windowRegulatorsCombo->currentIndex() >= 0) {
        dto.windowRegulators =
            static_cast<domain::WindowRegulators>(m_windowRegulatorsCombo->currentData().toInt());
    }
    if (m_airConditioningCombo->currentIndex() >= 0) {
        dto.airConditioning =
            static_cast<domain::AirConditioning>(m_airConditioningCombo->currentData().toInt());
    }

    // Solo viajan las filas marcadas: en vehicle_inspection la ausencia ya
    // significa "la unidad no lo trae", y el checklist completo se puede
    // reconstruir con un LEFT JOIN contra el catálogo.
    for (ConditionChecklistRow *row : m_checklistRows) {
        if (const std::optional<application::InspectionEntryDto> entry = row->value())
            dto.inspection << *entry;
    }
    return dto;
}

