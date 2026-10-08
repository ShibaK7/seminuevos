#include "presentation/views/support/formsupport.h"

#include <QAbstractButton>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QHash>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QTextDocument>
#include <QTextEdit>
#include <QVariant>
#include <QWidget>

namespace formsupport {
namespace {

// Las propiedades dinámicas de los campos con error. "field" la ponen las
// vistas y "hasError" la lee el QSS, así que sus nombres no se pueden cambiar
// solo aquí.
constexpr char kFieldProperty[] = "field";
constexpr char kHasErrorProperty[] = "hasError";
constexpr char kOriginalToolTipProperty[] = "originalToolTip";

QString fieldOf(const QWidget *widget)
{
    return widget->property(kFieldProperty).toString();
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

// Pone o quita la marca de error de un campo, junto con su tooltip. Sin
// mensajes, el campo queda limpio.
void applyFieldErrors(QWidget *widget, const QStringList &messages)
{
    const bool hasError = !messages.isEmpty();
    if (widget->property(kHasErrorProperty).toBool() != hasError) {
        widget->setProperty(kHasErrorProperty, hasError);
        repolish(widget);
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

void showFieldErrors(QWidget *root, const QList<domain::ValidationError> &errors)
{
    if (!root)
        return;

    // Agrupados por clave: un mismo campo puede traer más de un mensaje, como
    // el rechazo de un setter y una regla de validación.
    QHash<QString, QStringList> messagesByField;
    for (const domain::ValidationError &error : errors)
        messagesByField[error.field] << error.message;

    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        const QString field = fieldOf(widget);
        if (!field.isEmpty())
            applyFieldErrors(widget, messagesByField.value(field));
    }
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
