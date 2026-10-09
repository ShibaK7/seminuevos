#include "presentation/common/forms/formsupport.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QProxyStyle>
#include <QScrollArea>
#include <QScrollBar>
#include <QTest>
#include <QVBoxLayout>
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

QTEST_MAIN(TestFormSupport)

#include "tst_formsupport.moc"
