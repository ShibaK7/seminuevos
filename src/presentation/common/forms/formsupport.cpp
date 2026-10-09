#include "presentation/common/forms/formsupport.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHash>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QResource>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QTextDocument>
#include <QTextEdit>
#include <QVariant>
#include <QWidget>

#include <algorithm>

namespace formsupport {
namespace {

// Las propiedades dinámicas de los campos con error. "field" la ponen las
// vistas y "hasError" la lee el QSS, así que sus nombres no se pueden cambiar
// solo aquí.
constexpr char kFieldProperty[] = "field";
constexpr char kHasErrorProperty[] = "hasError";
constexpr char kOriginalToolTipProperty[] = "originalToolTip";
// La etiqueta de error de un campo, guardada en el campo.
constexpr char kErrorLabelProperty[] = "fieldErrorLabel";
// Marca de una rejilla que ya abrió sus renglones de errores.
constexpr char kErrorRowsProperty[] = "fieldErrorRows";
// La clase con la que el QSS pinta las etiquetas de error.
constexpr char kErrorLabelClass[] = "field-error";
// Aire, en píxeles, que deja focusField() arriba y abajo del campo al
// desplazar un área con scroll: alcanza para su mensaje de error.
constexpr int kScrollMargin = 40;

QString fieldOf(const QWidget *widget)
{
    return widget->property(kFieldProperty).toString();
}

QLabel *errorLabelOf(const QWidget *widget)
{
    return qobject_cast<QLabel *>(widget->property(kErrorLabelProperty).value<QObject *>());
}

bool isErrorLabel(const QWidget *widget)
{
    return widget->property("class").toString() == QLatin1String(kErrorLabelClass);
}

// El layout que tiene a widget como elemento directo, buscando dentro del
// layout de su padre y de los layouts anidados en él.
QLayout *layoutHolding(QLayout *layout, QWidget *widget)
{
    if (layout->indexOf(widget) >= 0)
        return layout;
    for (int i = 0; i < layout->count(); ++i) {
        if (QLayout *child = layout->itemAt(i)->layout()) {
            if (QLayout *found = layoutHolding(child, widget))
                return found;
        }
    }
    return nullptr;
}

QLayout *layoutHolding(QWidget *widget)
{
    QWidget *parent = widget->parentWidget();
    if (!parent || !parent->layout())
        return nullptr;
    return layoutHolding(parent->layout(), widget);
}

// Abre un renglón vacío debajo de cada renglón de la rejilla: el r pasa a ser
// el 2r y el 2r + 1 queda para los errores del de arriba. Un elemento que
// ocupaba n renglones pasa a ocupar 2n - 1, así sigue cubriendo los renglones
// de errores que quedan en medio. Solo la primera vez.
//
// QGridLayout no sabe insertar renglones, así que saca todo y lo vuelve a
// poner en su nuevo lugar. Los layouts anidados se vuelven a agregar con
// addLayout(): takeAt() les quita el padre y addItem() no se los devuelve.
void openErrorRows(QGridLayout *grid)
{
    if (grid->property(kErrorRowsProperty).toBool())
        return;
    grid->setProperty(kErrorRowsProperty, true);

    struct Cell
    {
        QLayoutItem *item = nullptr;
        int row = 0;
        int column = 0;
        int rowSpan = 1;
        int columnSpan = 1;
    };
    QList<Cell> cells;
    for (int i = 0; i < grid->count(); ++i) {
        Cell cell;
        cell.item = grid->itemAt(i);
        grid->getItemPosition(i, &cell.row, &cell.column, &cell.rowSpan, &cell.columnSpan);
        cells << cell;
    }

    const int rows = grid->rowCount();
    QList<int> stretches;
    QList<int> minimumHeights;
    for (int row = 0; row < rows; ++row) {
        stretches << grid->rowStretch(row);
        minimumHeights << grid->rowMinimumHeight(row);
    }

    while (grid->count() > 0)
        grid->takeAt(0);
    for (const Cell &cell : std::as_const(cells)) {
        const int row = 2 * cell.row;
        const int rowSpan = 2 * cell.rowSpan - 1;
        if (QLayout *child = cell.item->layout())
            grid->addLayout(child, row, cell.column, rowSpan, cell.columnSpan, child->alignment());
        else
            grid->addItem(cell.item, row, cell.column, rowSpan, cell.columnSpan,
                          cell.item->alignment());
    }

    for (int row = 0; row < rows; ++row) {
        grid->setRowStretch(row, 0);
        grid->setRowMinimumHeight(row, 0);
    }
    for (int row = 0; row < rows; ++row) {
        grid->setRowStretch(2 * row, stretches.at(row));
        grid->setRowMinimumHeight(2 * row, minimumHeights.at(row));
    }
}

// La etiqueta de error de un campo, todavía sin lugar en ningún layout.
// Arranca oculta: solo aparece cuando el campo tiene error.
QLabel *newErrorLabel(QWidget *field)
{
    auto *label = new QLabel(field->parentWidget());
    label->setObjectName(field->objectName() + QStringLiteral("Error"));
    label->setProperty("class", QLatin1String(kErrorLabelClass));
    // Texto plano: si un mensaje trae algo que capturó el usuario, se muestra
    // tal cual en vez de interpretarse como HTML.
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setVisible(false);
    field->setProperty(kErrorLabelProperty, QVariant::fromValue<QObject *>(label));
    // La etiqueta es del campo: si el campo desaparece, ella también.
    QObject::connect(field, &QObject::destroyed, label, &QObject::deleteLater);
    return label;
}

// Qt no vuelve a aplicar el QSS cuando cambia una propiedad dinámica: hay que
// despulir y pulir el widget para que se reevalúe [hasError="true"]. Con el
// contenedor basta: el editor que llevan adentro los combos editables y los
// spinboxes, y el viewport de un QTextEdit, se pintan con la regla de él.
void repolish(QWidget *widget)
{
    QStyle *style = widget->style();
    style->unpolish(widget);
    style->polish(widget);
    widget->update();
}

// Pone o quita la marca de error de un campo, junto con sus mensajes (en su
// etiqueta de error o, si no tiene, en su tooltip). Sin mensajes, el campo
// queda limpio.
void applyFieldErrors(QWidget *widget, const QStringList &messages)
{
    const bool hasError = !messages.isEmpty();
    if (widget->property(kHasErrorProperty).toBool() != hasError) {
        widget->setProperty(kHasErrorProperty, hasError);
        repolish(widget);
    }

    if (QLabel *label = errorLabelOf(widget)) {
        label->setText(messages.join(QLatin1Char('\n')));
        label->setVisible(hasError);
        return;
    }

    const QVariant originalToolTip = widget->property(kOriginalToolTipProperty);
    if (hasError) {
        // Solo la primera vez: si el campo ya tenía error, su tooltip actual son
        // los mensajes de antes, no el original.
        if (!originalToolTip.isValid())
            widget->setProperty(kOriginalToolTipProperty, widget->toolTip());
        // Se pasa de texto plano a HTML escapado: si un mensaje trae algo que
        // capturó el usuario, se muestra tal cual en vez de interpretarse como
        // etiquetas. Con WhiteSpaceNormal, un mensaje largo se parte en
        // renglones en vez de estirar el tooltip.
        widget->setToolTip(Qt::convertFromPlainText(messages.join(QLatin1Char('\n')),
                                                    Qt::WhiteSpaceNormal));
    } else if (originalToolTip.isValid()) {
        widget->setToolTip(originalToolTip.toString());
        // Se borra para que un error posterior vuelva a guardar el tooltip que
        // el campo tenga en ese momento.
        widget->setProperty(kOriginalToolTipProperty, QVariant());
    }
}

} // namespace

void watchEdits(QWidget *root, QObject *context, std::function<void()> onEdit)
{
    if (!root || !context || !onEdit)
        return;

    // Cada connect() guarda su propia copia de onEdit, así que no importa que
    // el parámetro deje de existir al salir de aquí.
    for (QLineEdit *edit : root->findChildren<QLineEdit *>())
        QObject::connect(edit, &QLineEdit::textChanged, context, onEdit);
    for (QTextEdit *edit : root->findChildren<QTextEdit *>())
        QObject::connect(edit, &QTextEdit::textChanged, context, onEdit);
    for (QPlainTextEdit *edit : root->findChildren<QPlainTextEdit *>())
        QObject::connect(edit, &QPlainTextEdit::textChanged, context, onEdit);

    for (QComboBox *combo : root->findChildren<QComboBox *>()) {
        QObject::connect(combo, &QComboBox::currentIndexChanged, context, onEdit);
        // Un combo editable (el de la marca, por ejemplo) cambia de texto
        // mientras se escribe sin cambiar de índice.
        QObject::connect(combo, &QComboBox::editTextChanged, context, onEdit);
    }

    for (QSpinBox *spin : root->findChildren<QSpinBox *>())
        QObject::connect(spin, &QSpinBox::valueChanged, context, onEdit);
    for (QDoubleSpinBox *spin : root->findChildren<QDoubleSpinBox *>())
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, context, onEdit);
    for (QDateEdit *edit : root->findChildren<QDateEdit *>())
        QObject::connect(edit, &QDateEdit::dateChanged, context, onEdit);

    // Solo los marcables: un botón común, como "Marcar todo", no es un dato
    // del formulario. Lo que sí avisa son las casillas que ese botón cambia.
    for (QAbstractButton *button : root->findChildren<QAbstractButton *>()) {
        if (button->isCheckable())
            QObject::connect(button, &QAbstractButton::toggled, context, onEdit);
    }
}

void addFieldErrorLabels(QWidget *root)
{
    if (!root)
        return;

    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    for (QWidget *field : widgets) {
        if (fieldOf(field).isEmpty() || errorLabelOf(field))
            continue;

        QLayout *layout = layoutHolding(field);
        if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
            openErrorRows(grid);
            int row = 0, column = 0, rowSpan = 1, columnSpan = 1;
            grid->getItemPosition(grid->indexOf(field), &row, &column, &rowSpan, &columnSpan);
            // El renglón de errores que sigue al último del campo.
            grid->addWidget(newErrorLabel(field), row + rowSpan, column, 1, columnSpan);
        } else if (auto *form = qobject_cast<QFormLayout *>(layout)) {
            int row = -1;
            QFormLayout::ItemRole role = QFormLayout::FieldRole;
            form->getWidgetPosition(field, &row, &role);
            if (role == QFormLayout::SpanningRole)
                form->insertRow(row + 1, newErrorLabel(field));
            else if (role == QFormLayout::FieldRole)
                form->insertRow(row + 1, static_cast<QWidget *>(nullptr), newErrorLabel(field));
        } else if (auto *box = qobject_cast<QBoxLayout *>(layout);
                   box && box->direction() == QBoxLayout::TopToBottom) {
            box->insertWidget(box->indexOf(field) + 1, newErrorLabel(field));
        }
    }
}

QList<domain::ValidationError> showFieldErrors(QWidget *root,
                                               const QList<domain::ValidationError> &errors)
{
    if (!root)
        return errors;

    // Agrupados por clave: un mismo campo puede traer más de un mensaje, como
    // el rechazo de un setter y una regla de validación.
    QHash<QString, QStringList> messagesByField;
    for (const domain::ValidationError &error : errors)
        messagesByField[error.field] << error.message;

    QSet<QString> shownFields;
    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        const QString field = fieldOf(widget);
        if (field.isEmpty())
            continue;
        applyFieldErrors(widget, messagesByField.value(field));
        // isVisibleTo() y no isVisible(), igual que en focusField(): responde
        // lo mismo aunque root todavía no esté en pantalla.
        if (widget->isVisibleTo(root))
            shownFields.insert(field);
    }

    QList<domain::ValidationError> unshown;
    for (const domain::ValidationError &error : errors) {
        if (!shownFields.contains(error.field))
            unshown << error;
    }
    return unshown;
}

void clearFieldErrors(QWidget *root)
{
    showFieldErrors(root, {});
}

bool focusField(QWidget *root, const QString &field)
{
    if (!root || field.isEmpty())
        return false;

    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (fieldOf(widget) != field)
            continue;
        // Uno deshabilitado no acepta el foco, y dárselo a uno oculto dejaría
        // al usuario tecleando en algo que no ve. isVisibleTo() y no
        // isVisible(): pregunta si el widget se vería con root en pantalla, así
        // que responde lo mismo aunque root todavía no se muestre.
        if (!widget->isEnabled() || !widget->isVisibleTo(root))
            continue;
        widget->setFocus(Qt::OtherFocusReason);
        // Dentro de un área con scroll (el Paso 1 completo, por ejemplo), el
        // campo puede estar fuera de la vista: setFocus() no desplaza. El
        // margen deja ver también su mensaje de error, que va debajo.
        for (QWidget *ancestor = widget->parentWidget(); ancestor;
             ancestor = ancestor->parentWidget()) {
            if (auto *area = qobject_cast<QScrollArea *>(ancestor))
                area->ensureWidgetVisible(widget, 0, kScrollMargin);
        }
        return true;
    }
    return false;
}

} // namespace formsupport

namespace formsupport {

void applyFloatingShadow(QWidget *widget, int xOffset, int yOffset, int blur, int opacity)
{
    if (!widget)
        return;
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setXOffset(xOffset);
    shadow->setYOffset(yOffset);
    shadow->setBlurRadius(blur);
    shadow->setColor(QColor(0, 0, 0, opacity));
    widget->setGraphicsEffect(shadow);
}

QString styleSheetResource(const QString &resourcePath)
{
    // Con QResource y no con QFile: es un recurso compilado en el ejecutable,
    // no un archivo en disco.
    static QHash<QString, QString> cache;
    const auto cached = cache.constFind(resourcePath);
    if (cached != cache.cend())
        return *cached;

    const QResource resource(resourcePath);
    const QString sheet =
        resource.isValid() ? QString::fromUtf8(resource.uncompressedData()) : QString();
    cache.insert(resourcePath, sheet);
    return sheet;
}

void alignGridColumns(const QList<QGridLayout *> &grids)
{
    int columns = 0;
    for (const QGridLayout *grid : grids)
        columns = std::max(columns, grid->columnCount());

    QList<int> widths(columns, 0);
    for (QGridLayout *grid : grids) {
        for (int i = 0; i < grid->count(); ++i) {
            int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
            grid->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
            QLayoutItem *item = grid->itemAt(i);
            if (columnSpan != 1 || (item->widget() && isErrorLabel(item->widget())))
                continue;
            // Del widget y no del renglón del layout: un renglón de un widget
            // oculto mide cero, y la sección que no se ve también cuenta.
            const int width = item->widget() ? item->widget()->sizeHint().width()
                                             : item->sizeHint().width();
            widths[column] = std::max(widths[column], width);
        }
    }

    for (QGridLayout *grid : grids) {
        for (int column = 0; column < columns; ++column)
            grid->setColumnMinimumWidth(column, widths.at(column));
    }
}

QLabel *requiredLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setText(QStringLiteral("%1 <span style='color: #D90429; font-weight: bold;'>*</span>")
                       .arg(text.toHtmlEscaped()));
    return label;
}

void fillCombo(QComboBox *combo, const QList<application::CatalogOptionDto> &options,
               const QString &placeholder)
{
    if (!combo)
        return;
    combo->clear();
    if (!placeholder.isEmpty())
        combo->setPlaceholderText(placeholder);
    for (const application::CatalogOptionDto &option : options)
        combo->addItem(option.name, option.id);
    combo->setCurrentIndex(-1);
}

} // namespace formsupport
