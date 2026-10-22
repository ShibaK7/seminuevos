#include "presentation/common/forms/formsupport.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QProxyStyle>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTextEdit>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

namespace {

// Cuenta las veces que se vuelve a aplicar el estilo a un widget. Sirve para
// comprobar que showFieldErrors() solo repule los campos cuyo error cambió.
class PolishCountingStyle : public QProxyStyle
{
public:
    void polish(QWidget *widget) override
    {
        ++polishCount;
        QProxyStyle::polish(widget);
    }
    using QProxyStyle::polish;

    int polishCount = 0;
};

QLineEdit *addField(QWidget *root, const QString &field)
{
    auto *edit = new QLineEdit(root);
    edit->setProperty("field", field);
    return edit;
}

domain::ValidationError error(const QString &field, const QString &message)
{
    return domain::ValidationError{field, message};
}

} // namespace

// Cómo el asistente marca los campos que el dominio rechazó: la propiedad
// hasError (que pinta el QSS), el tooltip con los mensajes y el foco.
class TestFormSupport : public QObject
{
    Q_OBJECT

private slots:
    void marksOnlyFieldsWithAnExactKey();
    void clearsFieldsThatNoLongerFail();
    void showsEveryMessageOfAFieldInItsToolTip();
    void restoresTheOriginalToolTip();
    void repolishesOnlyWhenTheErrorStateChanges();
    void focusesTheFirstWidgetThatCanTakeFocus();
    void toleratesANullRoot();

    // Los mensajes bajo los campos (addFieldErrorLabels).
    void opensAnErrorRowUnderEachGridRow();
    void putsTheErrorUnderTheFieldInFormAndBoxLayouts();
    void showsTheMessagesUnderTheFieldInsteadOfTheToolTip();
    void reportsTheErrorsItCannotShow();
    void focusScrollsTheFieldIntoView();

    // Solo lectura (el expediente en consulta) y campos bloqueados al editar.
    void readOnlyTextsStaySelectableButDoNotChange();
    void readOnlySpinBoxGetsItsArrowsBackOnlyOnce();
    void readOnlyDateDropsItsCalendarUntilEditable();
    void editableAgainDateLeavesRoomForItsCalendar();
    void readOnlyComboIgnoresClicksKeysAndWheel();
    void readOnlyCheckBoxDoesNotToggle();
    void lockedFieldsAreDisabledAndOnlyThoseComeBack();
    void readOnlyLeavesPlainButtonsAlone();
    void selectOptionKeepsAMissingCatalogEntry();
    void selectTextKeepsAMissingText();
};

void TestFormSupport::marksOnlyFieldsWithAnExactKey()
{
    QWidget root;
    QLineEdit *nested = addField(&root, QStringLiteral("counterparty.fullName"));
    QLineEdit *bare = addField(&root, QStringLiteral("fullName"));
    auto *untagged = new QLineEdit(&root);

    formsupport::showFieldErrors(&root, {error(QStringLiteral("counterparty.fullName"),
                                               QStringLiteral("Captura el nombre completo."))});

    QVERIFY(nested->property("hasError").toBool());
    // Mismo final de clave, pero no es la misma clave.
    QVERIFY(!bare->property("hasError").toBool());
    // Sin "field" no es un campo del formulario: ni se toca.
    QVERIFY(!untagged->property("hasError").isValid());
}

void TestFormSupport::clearsFieldsThatNoLongerFail()
{
    QWidget root;
    QLineEdit *serial = addField(&root, QStringLiteral("serialNumber"));
    QLineEdit *model = addField(&root, QStringLiteral("model"));

    formsupport::showFieldErrors(&root, {error(QStringLiteral("serialNumber"), QStringLiteral("a")),
                                         error(QStringLiteral("model"), QStringLiteral("b"))});
    QVERIFY(serial->property("hasError").toBool());
    QVERIFY(model->property("hasError").toBool());

    // Una nueva lista sustituye a la anterior: el modelo ya se corrigió.
    formsupport::showFieldErrors(&root, {error(QStringLiteral("serialNumber"), QStringLiteral("a"))});
    QVERIFY(serial->property("hasError").toBool());
    QVERIFY(!model->property("hasError").toBool());

    formsupport::clearFieldErrors(&root);
    QVERIFY(!serial->property("hasError").toBool());
    QVERIFY(!model->property("hasError").toBool());
}

void TestFormSupport::showsEveryMessageOfAFieldInItsToolTip()
{
    QWidget root;
    QLineEdit *model = addField(&root, QStringLiteral("model"));

    formsupport::showFieldErrors(&root, {error(QStringLiteral("model"), QStringLiteral("Primero")),
                                         error(QStringLiteral("serialNumber"), QStringLiteral("Ajeno")),
                                         error(QStringLiteral("model"), QStringLiteral("Segundo <b>"))});

    const QString toolTip = model->toolTip();
    QVERIFY2(toolTip.contains(QStringLiteral("Primero")), qPrintable(toolTip));
    QVERIFY2(toolTip.contains(QStringLiteral("Segundo")), qPrintable(toolTip));
    QVERIFY2(!toolTip.contains(QStringLiteral("Ajeno")), qPrintable(toolTip));
    // Lo que parece una etiqueta se muestra como texto, no se interpreta.
    QVERIFY2(toolTip.contains(QStringLiteral("&lt;b&gt;")), qPrintable(toolTip));
}

void TestFormSupport::restoresTheOriginalToolTip()
{
    QWidget root;
    QLineEdit *plates = addField(&root, QStringLiteral("plates"));
    plates->setToolTip(QStringLiteral("Como aparecen en la tarjeta de circulación"));

    formsupport::showFieldErrors(&root, {error(QStringLiteral("plates"), QStringLiteral("Uno"))});
    QVERIFY(plates->toolTip().contains(QStringLiteral("Uno")));

    // Un segundo error no debe guardar como original el tooltip del primero.
    formsupport::showFieldErrors(&root, {error(QStringLiteral("plates"), QStringLiteral("Dos"))});
    QVERIFY(plates->toolTip().contains(QStringLiteral("Dos")));

    formsupport::clearFieldErrors(&root);
    QCOMPARE(plates->toolTip(), QStringLiteral("Como aparecen en la tarjeta de circulación"));
    QVERIFY(!plates->property("originalToolTip").isValid());

    // Sin error, el tooltip es del campo: si cambia, el siguiente error guarda
    // y después devuelve el nuevo.
    plates->setToolTip(QStringLiteral("Otro"));
    formsupport::showFieldErrors(&root, {error(QStringLiteral("plates"), QStringLiteral("Tres"))});
    formsupport::clearFieldErrors(&root);
    QCOMPARE(plates->toolTip(), QStringLiteral("Otro"));
}

void TestFormSupport::repolishesOnlyWhenTheErrorStateChanges()
{
    // Antes que los widgets: setStyle() no se queda con el estilo, así que este
    // tiene que vivir más que ellos.
    PolishCountingStyle style;
    QWidget root;
    QLineEdit *serial = addField(&root, QStringLiteral("serialNumber"));
    auto *untagged = new QLineEdit(&root);

    serial->setStyle(&style);
    untagged->setStyle(&style);
    // setStyle() ya pule una vez cada widget; se cuenta a partir de aquí.
    style.polishCount = 0;

    formsupport::clearFieldErrors(&root);
    QCOMPARE(style.polishCount, 0);

    formsupport::showFieldErrors(&root, {error(QStringLiteral("serialNumber"), QStringLiteral("a"))});
    QCOMPARE(style.polishCount, 1);

    // Sigue con error, aunque el mensaje cambie: nada que repulir.
    formsupport::showFieldErrors(&root, {error(QStringLiteral("serialNumber"), QStringLiteral("b"))});
    QCOMPARE(style.polishCount, 1);

    formsupport::clearFieldErrors(&root);
    QCOMPARE(style.polishCount, 2);

    formsupport::clearFieldErrors(&root);
    QCOMPARE(style.polishCount, 2);
}

void TestFormSupport::focusesTheFirstWidgetThatCanTakeFocus()
{
    QWidget root;
    QLineEdit *hidden = addField(&root, QStringLiteral("basePrice"));
    hidden->hide();
    QLineEdit *disabled = addField(&root, QStringLiteral("basePrice"));
    disabled->setEnabled(false);
    QLineEdit *visible = addField(&root, QStringLiteral("basePrice"));
    addField(&root, QStringLiteral("model"));

    QVERIFY(formsupport::focusField(&root, QStringLiteral("basePrice")));
    // root nunca se muestra, así que no hay ventana activa: focusWidget() dice
    // a quién le tocará el foco cuando la haya.
    QCOMPARE(root.focusWidget(), visible);

    QVERIFY(!formsupport::focusField(&root, QStringLiteral("umaDailyValue")));
    QVERIFY(!formsupport::focusField(&root, QString()));
    QCOMPARE(root.focusWidget(), visible);
}

void TestFormSupport::toleratesANullRoot()
{
    formsupport::showFieldErrors(nullptr, {error(QStringLiteral("model"), QStringLiteral("a"))});
    formsupport::clearFieldErrors(nullptr);
    QVERIFY(!formsupport::focusField(nullptr, QStringLiteral("model")));
    formsupport::addFieldErrorLabels(nullptr);
}

namespace {

struct GridCell
{
    int row = -1;
    int column = -1;
    int rowSpan = 0;
    int columnSpan = 0;
};

GridCell cellOf(QGridLayout *grid, QWidget *widget)
{
    GridCell cell;
    grid->getItemPosition(grid->indexOf(widget), &cell.row, &cell.column, &cell.rowSpan,
                          &cell.columnSpan);
    return cell;
}

QLabel *errorLabelOf(QWidget *field)
{
    return field->parentWidget()->findChild<QLabel *>(field->objectName() + QStringLiteral("Error"));
}

} // namespace

void TestFormSupport::opensAnErrorRowUnderEachGridRow()
{
    // Como las tarjetas del Paso 1: etiqueta y campo, dos pares por renglón,
    // y un campo que ocupa tres columnas.
    QWidget root;
    auto *grid = new QGridLayout(&root);
    auto *modelLabel = new QLabel(QStringLiteral("Modelo:"), &root);
    QLineEdit *model = addField(&root, QStringLiteral("model"));
    model->setObjectName(QStringLiteral("modelEdit"));
    auto *colorLabel = new QLabel(QStringLiteral("Color:"), &root);
    QLineEdit *color = addField(&root, QStringLiteral("color"));
    color->setObjectName(QStringLiteral("colorEdit"));
    QLineEdit *description = addField(&root, QStringLiteral("description"));
    description->setObjectName(QStringLiteral("descriptionEdit"));
    grid->addWidget(modelLabel, 0, 0);
    grid->addWidget(model, 0, 1);
    grid->addWidget(colorLabel, 0, 2);
    grid->addWidget(color, 0, 3);
    grid->addWidget(description, 1, 1, 1, 3);
    grid->setRowStretch(1, 1);

    formsupport::addFieldErrorLabels(&root);

    // El renglón r pasa a ser el 2r, con todo y sus columnas.
    QCOMPARE(cellOf(grid, modelLabel).row, 0);
    QCOMPARE(cellOf(grid, model).row, 0);
    QCOMPARE(cellOf(grid, color).column, 3);
    QCOMPARE(cellOf(grid, description).row, 2);
    QCOMPARE(cellOf(grid, description).columnSpan, 3);
    // El estiramiento se va con su renglón.
    QCOMPARE(grid->rowStretch(2), 1);
    QCOMPARE(grid->rowStretch(1), 0);

    // El error, en el renglón de abajo y en las mismas columnas que su campo.
    QLabel *modelError = errorLabelOf(model);
    QVERIFY(modelError);
    QCOMPARE(cellOf(grid, modelError).row, 1);
    QCOMPARE(cellOf(grid, modelError).column, 1);
    QCOMPARE(cellOf(grid, errorLabelOf(color)).column, 3);
    const GridCell descriptionError = cellOf(grid, errorLabelOf(description));
    QCOMPARE(descriptionError.row, 3);
    QCOMPARE(descriptionError.columnSpan, 3);
    // Sin errores no se ven, y la clase es la que pinta el QSS.
    QVERIFY(!modelError->isVisibleTo(&root));
    QCOMPARE(modelError->property("class").toString(), QStringLiteral("field-error"));

    // Llamarla otra vez no abre más renglones ni duplica etiquetas.
    formsupport::addFieldErrorLabels(&root);
    QCOMPARE(cellOf(grid, description).row, 2);
    QCOMPARE(root.findChildren<QLabel *>(QStringLiteral("modelEditError")).size(), 1);
}

void TestFormSupport::putsTheErrorUnderTheFieldInFormAndBoxLayouts()
{
    QWidget root;
    auto *box = new QVBoxLayout(&root);
    auto *formHolder = new QWidget(&root);
    auto *form = new QFormLayout(formHolder);
    QLineEdit *fuel = addField(formHolder, QStringLiteral("conditions.fuelType"));
    fuel->setObjectName(QStringLiteral("fuelEdit"));
    QLineEdit *cylinders = addField(formHolder, QStringLiteral("conditions.cylinders"));
    cylinders->setObjectName(QStringLiteral("cylindersEdit"));
    form->addRow(QStringLiteral("Combustible"), fuel);
    form->addRow(QStringLiteral("Cilindros"), cylinders);
    box->addWidget(formHolder);
    QLineEdit *checklist = addField(&root, QStringLiteral("inspection"));
    checklist->setObjectName(QStringLiteral("checklistEdit"));
    box->addWidget(checklist);
    box->addWidget(new QLabel(QStringLiteral("Al final"), &root));

    formsupport::addFieldErrorLabels(&root);

    // En el formulario, una fila nueva debajo, en la columna de los campos.
    int row = -1;
    QFormLayout::ItemRole role = QFormLayout::LabelRole;
    form->getWidgetPosition(errorLabelOf(fuel), &row, &role);
    QCOMPARE(row, 1);
    QCOMPARE(role, QFormLayout::FieldRole);
    form->getWidgetPosition(cylinders, &row, &role);
    QCOMPARE(row, 2);
    form->getWidgetPosition(errorLabelOf(cylinders), &row, &role);
    QCOMPARE(row, 3);

    // En un layout vertical, justo después del campo.
    QCOMPARE(box->indexOf(errorLabelOf(checklist)), box->indexOf(checklist) + 1);
}

void TestFormSupport::showsTheMessagesUnderTheFieldInsteadOfTheToolTip()
{
    QWidget root;
    auto *grid = new QGridLayout(&root);
    QLineEdit *plates = addField(&root, QStringLiteral("plates"));
    plates->setObjectName(QStringLiteral("platesEdit"));
    plates->setToolTip(QStringLiteral("Como aparecen en la tarjeta de circulación"));
    grid->addWidget(plates, 0, 1);
    formsupport::addFieldErrorLabels(&root);
    QLabel *platesError = errorLabelOf(plates);

    formsupport::showFieldErrors(&root, {error(QStringLiteral("plates"), QStringLiteral("Uno")),
                                         error(QStringLiteral("plates"), QStringLiteral("<b>Dos</b>"))});
    QVERIFY(plates->property("hasError").toBool());
    QVERIFY(platesError->isVisibleTo(&root));
    // Un mensaje por renglón, en texto plano: lo que capturó el usuario no se
    // interpreta como HTML.
    QCOMPARE(platesError->text(), QStringLiteral("Uno\n<b>Dos</b>"));
    QCOMPARE(platesError->textFormat(), Qt::PlainText);
    // El mensaje ya se ve: el tooltip sigue siendo el del campo.
    QCOMPARE(plates->toolTip(), QStringLiteral("Como aparecen en la tarjeta de circulación"));

    formsupport::clearFieldErrors(&root);
    QVERIFY(!plates->property("hasError").toBool());
    QVERIFY(!platesError->isVisibleTo(&root));
}

void TestFormSupport::reportsTheErrorsItCannotShow()
{
    QWidget root;
    addField(&root, QStringLiteral("model"));
    // El de la otra rama: existe, pero oculto.
    addField(&root, QStringLiteral("basePrice"))->hide();

    const QList<domain::ValidationError> unshown = formsupport::showFieldErrors(
        &root, {error(QStringLiteral("model"), QStringLiteral("a")),
                error(QStringLiteral("umaDailyValue"), QStringLiteral("b")),
                error(QStringLiteral("basePrice"), QStringLiteral("c"))});

    // En el orden en que llegaron.
    QCOMPARE(unshown.size(), 2);
    QCOMPARE(unshown.at(0).field, QStringLiteral("umaDailyValue"));
    QCOMPARE(unshown.at(1).field, QStringLiteral("basePrice"));
    QCOMPARE(formsupport::showFieldErrors(nullptr, unshown).size(), 2);
}

void TestFormSupport::focusScrollsTheFieldIntoView()
{
    QScrollArea area;
    area.setWidgetResizable(true);
    area.resize(300, 120);
    auto *content = new QWidget;
    auto *box = new QVBoxLayout(content);
    for (int i = 0; i < 20; ++i)
        box->addWidget(new QLineEdit(content));
    QLineEdit *last = addField(content, QStringLiteral("observations"));
    box->addWidget(last);
    area.setWidget(content);
    area.show();
    QVERIFY(QTest::qWaitForWindowExposed(&area));
    QCOMPARE(area.verticalScrollBar()->value(), 0);

    QVERIFY(formsupport::focusField(content, QStringLiteral("observations")));
    // El campo quedó dentro de lo que se ve.
    const QRect visible = area.viewport()->rect();
    const QRect field(last->mapTo(area.viewport(), QPoint(0, 0)), last->size());
    QVERIFY(area.verticalScrollBar()->value() > 0);
    QVERIFY2(visible.contains(field), qPrintable(QStringLiteral("campo en y=%1, visible hasta %2")
                                                      .arg(field.top())
                                                      .arg(visible.bottom())));
}

// En consulta un texto se puede seleccionar y copiar (el VIN, por ejemplo),
// pero no cambia; al editar vuelve a ser un campo normal.
void TestFormSupport::readOnlyTextsStaySelectableButDoNotChange()
{
    QWidget root;
    auto *layout = new QVBoxLayout(&root);
    auto *line = new QLineEdit(QStringLiteral("ABC123"), &root);
    auto *text = new QTextEdit(&root);
    text->setPlainText(QStringLiteral("nota"));
    layout->addWidget(line);
    layout->addWidget(text);
    root.show();

    formsupport::setReadOnly(&root, true);
    QVERIFY(line->isReadOnly());
    QVERIFY(text->isReadOnly());
    QVERIFY(line->isEnabled());
    QTest::keyClicks(line, QStringLiteral("XYZ"));
    QCOMPARE(line->text(), QStringLiteral("ABC123"));

    formsupport::setReadOnly(&root, false);
    QVERIFY(!line->isReadOnly());
    QVERIFY(!text->isReadOnly());
}

// Las flechas se quitan en consulta y vuelven al editar, aunque la consulta
// se haya aplicado dos veces (al refrescar el expediente): la segunda no debe
// guardar "sin flechas" como lo original.
void TestFormSupport::readOnlySpinBoxGetsItsArrowsBackOnlyOnce()
{
    QWidget root;
    auto *spin = new QSpinBox(&root);
    spin->setRange(0, 100);
    spin->setValue(5);
    QCOMPARE(spin->buttonSymbols(), QAbstractSpinBox::UpDownArrows);

    formsupport::setReadOnly(&root, true);
    QVERIFY(spin->isReadOnly());
    QCOMPARE(spin->buttonSymbols(), QAbstractSpinBox::NoButtons);
    formsupport::setReadOnly(&root, true);
    formsupport::setReadOnly(&root, false);
    QVERIFY(!spin->isReadOnly());
    QCOMPARE(spin->buttonSymbols(), QAbstractSpinBox::UpDownArrows);
}

// Una fecha con calendario se pintaría como deshabilitada en solo lectura: se
// queda sin calendario mientras lo es, y lo recupera una sola vez al volver.
void TestFormSupport::readOnlyDateDropsItsCalendarUntilEditable()
{
    QWidget root;
    auto *date = new QDateEdit(&root);
    date->setCalendarPopup(true);
    auto *plain = new QDateEdit(&root);
    QVERIFY(!plain->calendarPopup());

    formsupport::setReadOnly(&root, true);
    QVERIFY(date->isReadOnly());
    QVERIFY(!date->calendarPopup());
    formsupport::setReadOnly(&root, true);
    formsupport::setReadOnly(&root, false);
    QVERIFY(date->calendarPopup());
    QVERIFY(!plain->calendarPopup());
}

// Con las reglas de la hoja de la app (el lugar del calendario a la derecha,
// que en solo lectura se quita), una fecha que vuelve a ser editable deja su
// campo interno igual que una que nunca dejó de serlo. Si el campo se quedara
// con el ancho de la solo lectura, taparía el ícono y el clic no abriría el
// calendario.
void TestFormSupport::editableAgainDateLeavesRoomForItsCalendar()
{
    QWidget window;
    window.setStyleSheet(QStringLiteral(
        "QDateEdit { padding: 6px 56px 6px 14px; }"
        "QDateEdit[readOnly=\"true\"] { padding-right: 14px; }"
        "QDateEdit::drop-down { subcontrol-origin: margin; subcontrol-position: center right;"
        "  width: 24px; margin-right: 8px; border: none; }"
        "QDateEdit[readOnly=\"true\"]::drop-down { width: 0px; margin: 0px; }"));
    auto *layout = new QVBoxLayout(&window);
    auto *form = new QWidget(&window);
    auto *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    auto *date = new QDateEdit(form);
    date->setCalendarPopup(true);
    formLayout->addWidget(date);
    auto *twin = new QDateEdit(&window);
    twin->setCalendarPopup(true);
    layout->addWidget(form);
    layout->addWidget(twin);
    window.resize(400, 120);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    formsupport::setReadOnly(form, true);
    formsupport::setReadOnly(form, false);
    QCoreApplication::processEvents();
    QCOMPARE(date->width(), twin->width());
    QCOMPARE(date->findChild<QLineEdit *>()->geometry(), twin->findChild<QLineEdit *>()->geometry());
}

// QComboBox no tiene readOnly: la guarda se traga flechas y clics, y la rueda
// (que cambiaría la opción al desplazar la página) tampoco la mueve.
void TestFormSupport::readOnlyComboIgnoresClicksKeysAndWheel()
{
    QWidget root;
    auto *layout = new QVBoxLayout(&root);
    auto *combo = new QComboBox(&root);
    combo->addItems({QStringLiteral("Uno"), QStringLiteral("Dos"), QStringLiteral("Tres")});
    combo->setCurrentIndex(1);
    layout->addWidget(combo);
    root.show();

    formsupport::setReadOnly(&root, true);
    QVERIFY(combo->property("readOnly").toBool());
    QTest::keyClick(combo, Qt::Key_Down);
    QTest::keyClick(combo, Qt::Key_Up);
    QCOMPARE(combo->currentIndex(), 1);

    const QPointF center = QRectF(combo->rect()).center();
    QWheelEvent wheel(center, combo->mapToGlobal(center), QPoint(), QPoint(0, -120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(combo, &wheel);
    QCOMPARE(combo->currentIndex(), 1);

    QTest::mouseClick(combo, Qt::LeftButton);
    QVERIFY(!combo->view()->isVisible());
    combo->hidePopup();

    formsupport::setReadOnly(&root, false);
    QVERIFY(!combo->property("readOnly").toBool());
    QTest::keyClick(combo, Qt::Key_Down);
    QCOMPARE(combo->currentIndex(), 2);
}

// Una casilla de solo lectura se ve igual (no en gris) pero no cambia.
void TestFormSupport::readOnlyCheckBoxDoesNotToggle()
{
    QWidget root;
    auto *layout = new QVBoxLayout(&root);
    auto *check = new QCheckBox(QStringLiteral("Luces delanteras"), &root);
    check->setChecked(true);
    layout->addWidget(check);
    root.show();

    formsupport::setReadOnly(&root, true);
    QTest::mouseClick(check, Qt::LeftButton);
    QTest::keyClick(check, Qt::Key_Space);
    QVERIFY(check->isChecked());
    QVERIFY(check->isEnabled());

    formsupport::setReadOnly(&root, false);
    QTest::mouseClick(check, Qt::LeftButton);
    QVERIFY(!check->isChecked());
}

// Al editar, los campos de lockedFields se deshabilitan con un tooltip que lo
// explica. De vuelta en consulta, solo se rehabilitan esos: Folio, que viene
// deshabilitado del .ui, nunca se habilita.
void TestFormSupport::lockedFieldsAreDisabledAndOnlyThoseComeBack()
{
    QWidget root;
    QLineEdit *type = addField(&root, QStringLiteral("acquisitionType"));
    type->setToolTip(QStringLiteral("original"));
    QLineEdit *folio = addField(&root, QStringLiteral("folio"));
    folio->setEnabled(false);
    QLineEdit *model = addField(&root, QStringLiteral("model"));

    const QStringList locked{QStringLiteral("acquisitionType"), QStringLiteral("folio")};
    formsupport::setReadOnly(&root, false, locked);
    QVERIFY(!type->isEnabled());
    QVERIFY(type->toolTip() != QStringLiteral("original"));
    QVERIFY(!folio->isEnabled());
    QVERIFY(model->isEnabled());
    QVERIFY(!model->isReadOnly());

    formsupport::setReadOnly(&root, true);
    QVERIFY(type->isEnabled());
    QVERIFY(type->isReadOnly());
    QCOMPARE(type->toolTip(), QStringLiteral("original"));
    QVERIFY(!folio->isEnabled());
}

// Los botones normales ("Ver", "Subir documento") no son campos: cada vista
// decide si los oculta.
void TestFormSupport::readOnlyLeavesPlainButtonsAlone()
{
    QWidget root;
    auto *button = new QPushButton(QStringLiteral("Ver"), &root);
    formsupport::setReadOnly(&root, true);
    QVERIFY(button->isEnabled());
    QVERIFY(!button->property("readOnly").isValid());
}

// Un dato guardado se ve tal cual aunque su opción ya no esté en el catálogo.
void TestFormSupport::selectOptionKeepsAMissingCatalogEntry()
{
    QComboBox combo;
    formsupport::fillCombo(&combo, {{1, QStringLiteral("Ford"), -1}, {2, QStringLiteral("Nissan"), -1}});
    formsupport::selectOption(&combo, domain::CatalogRef{2, QStringLiteral("Nissan")});
    QCOMPARE(combo.currentText(), QStringLiteral("Nissan"));

    formsupport::selectOption(&combo, domain::CatalogRef{9, QStringLiteral("Saturn")});
    QCOMPARE(combo.currentText(), QStringLiteral("Saturn"));
    QCOMPARE(combo.currentData().toInt(), 9);

    formsupport::selectOption(&combo, domain::CatalogRef{});
    QCOMPARE(combo.currentIndex(), -1);
}

void TestFormSupport::selectTextKeepsAMissingText()
{
    QComboBox combo;
    combo.addItems({QStringLiteral("4"), QStringLiteral("6")});
    formsupport::selectText(&combo, QStringLiteral("6"));
    QCOMPARE(combo.currentIndex(), 1);
    formsupport::selectText(&combo, QStringLiteral("12"));
    QCOMPARE(combo.currentText(), QStringLiteral("12"));
    formsupport::selectText(&combo, QString());
    QCOMPARE(combo.currentIndex(), -1);
}

QTEST_MAIN(TestFormSupport)

#include "tst_formsupport.moc"
