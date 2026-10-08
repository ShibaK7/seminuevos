#include "presentation/views/support/formsupport.h"

#include <QLineEdit>
#include <QObject>
#include <QProxyStyle>
#include <QTest>
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
}

QTEST_MAIN(TestFormSupport)

#include "tst_formsupport.moc"
