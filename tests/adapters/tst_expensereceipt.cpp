// El recibo de pago de un gasto con su plantilla real: cada marca [Fecha],
// [Total], [Total_Letra], [Tipo_Gasto] y [Descripción] se sustituye con los
// datos del renglón, escapados, y el PDF se genera con las imágenes del
// recurso.

#include "adapters/contract/pdfexpensereceiptgenerator.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace {

application::OperationalExpenseItemDto expense()
{
    application::OperationalExpenseItemDto item;
    item.folio = 12;
    item.date = QDate(2026, 11, 24);
    item.typeName = QStringLiteral("Papelería");
    item.description = QStringLiteral("Hojas <carta> & tóner\nCarpetas");
    item.subtotal = 1000.0;
    item.iva = 160.0;
    item.total = 1160.0;
    item.paymentMethod = domain::PaymentMethod::Efectivo;
    return item;
}

} // namespace

class TstExpenseReceipt : public QObject
{
    Q_OBJECT

private slots:
    void everyMarkIsReplacedWithTheRowData()
    {
        QString error;
        const QString html = PdfExpenseReceiptGenerator::fillTemplate(expense(), &error);
        QVERIFY2(!html.isEmpty(), qPrintable(error));

        QVERIFY(html.contains(QStringLiteral("Ver. A 24 de noviembre de 2026.")));
        // Las dos veces que aparece el total, después del "$" de la plantilla.
        QCOMPARE(html.count(QStringLiteral("$1,160.00")), 2);
        // La misma conversión a letra que el recibo de mantenimiento.
        QVERIFY(html.contains(QStringLiteral("MIL CIENTO SESENTA PESOS 00/100 M.N.")));
        QVERIFY(html.contains(QStringLiteral("tipo de gasto Papelería el cual")));
        // Texto plano: se escapa, y los saltos de línea se conservan.
        QVERIFY(html.contains(QStringLiteral("Hojas &lt;carta&gt; &amp; tóner<br/>Carpetas")));
        for (const QString &mark : {QStringLiteral("[Fecha]"), QStringLiteral("[Total]"),
                                    QStringLiteral("[Total_Letra]"),
                                    QStringLiteral("[Tipo_Gasto]"),
                                    QStringLiteral("[Descripción]")})
            QVERIFY2(!html.contains(mark), qPrintable(mark));
    }

    void generatesThePdf()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("recibo.pdf"));

        PdfExpenseReceiptGenerator generator;
        const auto outcome = generator.generate(expense(), path);

        QVERIFY2(outcome.ok, qPrintable(outcome.errorMessage));
        QVERIFY(QFileInfo(path).size() > 0);
    }
};

QTEST_MAIN(TstExpenseReceipt)
#include "tst_expensereceipt.moc"
