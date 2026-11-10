#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "ui_vehicleconditionsview.h"

#include "presentation/inventory/registration/steps/conditions/conditionchecklistrow.h"
#include "presentation/common/forms/formsupport.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <optional>

VehicleConditionsView::VehicleConditionsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VehicleConditionsView)
{
    // El .ui arma las dos tarjetas: especificaciones básicas (stretch 0) y
    // checklist (stretch 1).
    //
    // Especificaciones: cada combo lleva en "field" la clave con la que el
    // dominio reporta sus errores: así el asistente sabe cuál marcar en rojo y
    // a cuál darle el foco. Las condiciones llegan con el prefijo
    // "conditions." porque así las agrupa
    // VehicleBuilder::validateConditionData(). Todos arrancan sin elegir
    // (currentIndex -1) con el placeholder "Seleccione una opción...".
    // Cilindros e Interiores traen sus opciones en el .ui, marcadas como no
    // traducibles: su texto es el dato que viaja a la base.
    //
    // El aire bajo el título lo pone un espaciador fijo de 15 px en el layout
    // y no una hoja de estilos propia del título: esa hoja le gana a la global
    // en ese widget y deja estilo regado por el código, fuera del QSS. El
    // layout no suma su espaciado alrededor de un espacio fijo, así que la
    // separación total queda igual que antes: estos 15 px más el espaciado
    // normal entre elementos.
    //
    // Checklist: la clave "field" va en el área de scroll completa
    // (checklistScrollArea) y no en cada fila: el dominio reporta los
    // renglones de la inspección por su posición entre los marcados
    // ("inspection.items[2]..."), no por fila de la pantalla, así que ninguna
    // fila tiene una clave fija que llevar.
    //
    // El contenedor del checklist (checklistContent) viene vacío y se puebla
    // en setChecklist(). OJO: el espaciador final NO va en el .ui -- tiene que
    // quedar después de las filas, o las empuja al fondo del área desplazable.
    ui->setupUi(this);

    // Las tarjetas se llaman cardPanel en tiempo de ejecución porque así las
    // encuentra el QSS global (QFrame#cardPanel). En el .ui no pueden llevar
    // ese nombre las dos: un formulario no admite nombres repetidos (uic deja
    // la segunda como "cardPanel1" y Designer la renombra al abrirla), así
    // que ahí tienen nombre propio y se renombran aquí, antes de que la hoja
    // de estilos las pula. La sombra tampoco cabe en el .ui: Designer no la
    // puede declarar.
    for (QFrame *card : {ui->specsCard, ui->checklistCard}) {
        card->setObjectName(QStringLiteral("cardPanel"));
        formsupport::applyFloatingShadow(card);
    }

    // Los combos de valores cerrados se llenan desde el dominio, que es la
    // misma fuente que decide qué texto acepta la base. Antes eran literales
    // repetidos aquí, y cualquier retoque a una etiqueta rompía el CHECK. En
    // el .ui van vacíos y con su placeholder ya puesto, así que siguen sin
    // elegir después de llenarlos.
    for (domain::Transmission value : domain::allTransmissions())
        ui->transmissionCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    for (domain::WindowRegulators value : domain::allWindowRegulators())
        ui->windowRegulatorsCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    for (domain::AirConditioning value : domain::allAirConditioningModes())
        ui->airConditioningCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    // Mensaje del checklist: arranca oculto, solo aparece con
    // showChecklistMessage().
    ui->checklistMessageLabel->setVisible(false);

    // Bajo cada campo, el lugar de sus mensajes de error. Sin errores no
    // ocupa espacio.
    formsupport::addFieldErrorLabels(this);

    // Cualquier cambio se avisa; el presenter decide cuándo revalidar. Las
    // filas del checklist se vigilan al crearlas (setChecklist).
    formsupport::watchEdits(this, this, [this] { emit edited(); });
}

VehicleConditionsView::~VehicleConditionsView()
{
    delete ui;
}

void VehicleConditionsView::showChecklistMessage(const QString &message)
{
    ui->checklistMessageLabel->setText(message);
    ui->checklistMessageLabel->setVisible(true);
}

void VehicleConditionsView::showConditions(const application::VehicleConditionsDto &conditions)
{
    formsupport::selectOption(ui->fuelTypeCombo, conditions.fuelType);
    formsupport::selectText(ui->cylindersCombo, conditions.cylinders
                                                    ? QString::number(*conditions.cylinders)
                                                    : QString());
    formsupport::selectEnum(ui->transmissionCombo, conditions.transmission);
    formsupport::selectText(ui->interiorMaterialCombo, conditions.interiorMaterial);
    formsupport::selectEnum(ui->windowRegulatorsCombo, conditions.windowRegulators);
    formsupport::selectEnum(ui->airConditioningCombo, conditions.airConditioning);

    m_shownInspection = conditions.inspection;
    applyShownInspection();
}

void VehicleConditionsView::applyShownInspection()
{
    if (!m_shownInspection)
        return;
    // Una fila sin renglón guardado es un elemento que la unidad no trae (o
    // que se agregó al catálogo después de guardarla): va desmarcada.
    for (ConditionChecklistRow *row : std::as_const(m_checklistRows)) {
        std::optional<application::InspectionEntryDto> entry;
        for (const application::InspectionEntryDto &stored : std::as_const(*m_shownInspection)) {
            if (stored.elementId == row->elementId()) {
                entry = stored;
                break;
            }
        }
        row->showEntry(entry);
    }
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
    formsupport::fillCombo(ui->fuelTypeCombo, fuelTypes);
}

QList<domain::ValidationError>
VehicleConditionsView::showFieldErrors(const QList<domain::ValidationError> &errors)
{
    return formsupport::showFieldErrors(this, errors);
}

bool VehicleConditionsView::focusField(const QString &field)
{
    return formsupport::focusField(this, field);
}

void VehicleConditionsView::setReadOnly(bool readOnly, const QStringList &lockedFields)
{
    m_readOnly = readOnly;
    formsupport::setReadOnly(ui->specsCard, readOnly, lockedFields);
    for (ConditionChecklistRow *row : std::as_const(m_checklistRows))
        row->setReadOnly(readOnly);
    for (QWidget *button : std::as_const(m_markAllButtons))
        button->setVisible(!readOnly);
}

void VehicleConditionsView::clearChecklist()
{
    // Se borran en el acto, no con deleteLater(): esto nunca corre dentro de
    // una señal de las filas, y así watchEdits() no vuelve a conectar las
    // viejas.
    while (QLayoutItem *item = ui->checklistLayout->takeAt(0)) {
        if (QLayout *header = item->layout()) {
            while (QLayoutItem *child = header->takeAt(0)) {
                delete child->widget();
                delete child;
            }
            // item y header son el mismo objeto: un layout agregado con
            // addLayout() es su propio QLayoutItem.
            delete header;
        } else {
            delete item->widget();
            delete item;
        }
    }
    m_checklistRows.clear();
    m_markAllButtons.clear();
}

void VehicleConditionsView::setChecklist(const QList<application::ChecklistItemDto> &items)
{
    // Si ya había un checklist (los catálogos se volvieron a pedir), se
    // reemplaza: sumar filas repetiría renglones en la inspección.
    clearChecklist();
    // Con el catálogo ya aquí, un aviso de que no estaba disponible (de una
    // lectura anterior que falló) ya no es cierto.
    ui->checklistMessageLabel->setVisible(false);

    const ConditionChecklistRow::ColumnWidths columns =
        ConditionChecklistRow::measureColumns(items);

    // Un solo recorrido: el catálogo ya viene ordenado y con las categorías
    // contiguas, así que basta abrir un encabezado cada vez que cambia.
    QString currentCategory;
    for (const application::ChecklistItemDto &item : items) {
        if (item.category != currentCategory) {
            if (!currentCategory.isEmpty()) {
                auto *separator = new QFrame(ui->checklistContent);
                separator->setFrameShape(QFrame::HLine);
                ui->checklistLayout->addWidget(separator);
            }
            currentCategory = item.category;

            auto *groupHeaderLayout = new QHBoxLayout;
            auto *groupLabel = new QLabel(currentCategory, ui->checklistContent);
            groupLabel->setProperty("class", QStringLiteral("h4"));
            auto *markAllButton = new QPushButton(QStringLiteral("Marcar todo"), ui->checklistContent);
            markAllButton->setProperty("class", QStringLiteral("secondary"));
            const QString category = currentCategory;
            connect(markAllButton, &QPushButton::clicked, this,
                    [this, category]() { markAllInGroup(category, true); });
            // En consulta no hay nada que marcar.
            markAllButton->setVisible(!m_readOnly);
            m_markAllButtons << markAllButton;

            groupHeaderLayout->addWidget(groupLabel);
            groupHeaderLayout->addStretch();
            groupHeaderLayout->addWidget(markAllButton);
            ui->checklistLayout->addLayout(groupHeaderLayout);
        }

        auto *row = new ConditionChecklistRow(item, columns, ui->checklistContent);
        // Las filas nacen con el modo del paso: el checklist llega con los
        // catálogos, a veces después de que el paso se puso en solo lectura.
        row->setReadOnly(m_readOnly);
        ui->checklistLayout->addWidget(row);
        m_checklistRows << row;
    }

    auto *separator = new QFrame(ui->checklistContent);
    separator->setFrameShape(QFrame::HLine);
    ui->checklistLayout->addWidget(separator);
    ui->checklistLayout->addStretch();

    // Si lo guardado llegó antes que el catálogo, se pinta ahora, antes de
    // vigilar las filas: pintar no es una edición.
    applyShownInspection();

    // Las filas nacen después del constructor, así que se vigilan aquí.
    formsupport::watchEdits(ui->checklistContent, this, [this] { emit edited(); });
}

application::VehicleConditionsDto VehicleConditionsView::conditions() const
{
    application::VehicleConditionsDto dto;

    const QVariant fuelData = ui->fuelTypeCombo->currentData();
    if (ui->fuelTypeCombo->currentIndex() >= 0 && fuelData.isValid() && !fuelData.isNull())
        dto.fuelType.id = fuelData.toInt();
    dto.fuelType.name = ui->fuelTypeCombo->currentText();

    // Un combo sin elegir (índice -1) queda vacío: así el dominio lo reporta
    // como faltante. Leerlo de todos modos convertía el userData vacío en 0, es
    // decir, en el primer valor del enum.
    if (ui->cylindersCombo->currentIndex() >= 0)
        dto.cylinders = ui->cylindersCombo->currentText().toInt();
    if (ui->transmissionCombo->currentIndex() >= 0)
        dto.transmission = static_cast<domain::Transmission>(ui->transmissionCombo->currentData().toInt());
    if (ui->interiorMaterialCombo->currentIndex() >= 0)
        dto.interiorMaterial = ui->interiorMaterialCombo->currentText();
    if (ui->windowRegulatorsCombo->currentIndex() >= 0) {
        dto.windowRegulators =
            static_cast<domain::WindowRegulators>(ui->windowRegulatorsCombo->currentData().toInt());
    }
    if (ui->airConditioningCombo->currentIndex() >= 0) {
        dto.airConditioning =
            static_cast<domain::AirConditioning>(ui->airConditioningCombo->currentData().toInt());
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

