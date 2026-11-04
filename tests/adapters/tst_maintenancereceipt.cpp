// El recibo de un mantenimiento (#47) con su plantilla real: cada marca
// [Fecha], [Costo], [Tipo_Mantenimiento] y [Descripción] se sustituye con los
// datos del renglón, escapados, y el PDF se genera con las imágenes del
// recurso.

#include "adapters/contract/pdfmaintenancereceiptgenerator.h"
#include "adapters/contract/paymentnotetemplate.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

namespace {

application::MaintenanceItemDto maintenance()
{
    application::MaintenanceItemDto item;
    item.id = 4;
    item.date = QDate(2026, 10, 19);
    item.typeName = QStringLiteral("Hojalatería y Pintura");
    item.cost = 12345.5;
    item.description = QStringLiteral("Pintura de <cofre> & defensa\nPulido");
    return item;
}

} // namespace

class TstMaintenanceReceipt : public QObject
{
    Q_OBJECT

private slots:
    void fillsAndExportsPaymentNoteTemplate()
    {
        application::CommercialSaleDetailsDto sale;
        sale.saleDate = QDate(2026, 11, 3);
        sale.buyerName = QStringLiteral("Ana <López>");
        sale.buyerAddress = QStringLiteral("Calle Uno 10");
        sale.buyerPopulation = QStringLiteral("Martínez de la Torre, Veracruz");
        sale.guarantorName = QStringLiteral("Luis Pérez");
        sale.guarantorAddress = QStringLiteral("Calle Dos 20");
        sale.guarantorPopulation = QStringLiteral("Veracruz, Veracruz");
        sale.guarantorPhone = QStringLiteral("2321234567");
        sale.expectedInstallments = 24;

        application::CommercialPaymentDto payment;
        payment.folio = 345;
        payment.saleFolio = 91;
        payment.installmentNumber = 3;
        payment.expectedAmount = 13125.50;
        payment.scheduledDate = QDate(2027, 2, 3);

        QCOMPARE(PaymentNoteTemplate::suggestedFileName(payment),
                 QStringLiteral("pagare_91_3.pdf"));

        QString error;
        const QString html = PaymentNoteTemplate::fillTemplate(sale, payment, &error);
        QVERIFY2(!html.isEmpty(), qPrintable(error));
        QVERIFY(html.contains(QStringLiteral("3/24")));
        QVERIFY(html.contains(QStringLiteral("$13,125.50")));
        QVERIFY(html.contains(QStringLiteral("TRECE MIL CIENTO VEINTICINCO")));
        QVERIFY(html.contains(QStringLiteral("50/100 M.N.")));
        QVERIFY(html.contains(QStringLiteral("3 de noviembre de 2026")));
        QVERIFY(html.contains(QStringLiteral("3 de febrero de 2027")));
        QVERIFY(html.contains(QStringLiteral("Ana &lt;López&gt;")));
        QVERIFY(html.contains(QStringLiteral("Luis Pérez")));
        QVERIFY(html.contains(QStringLiteral("background-color: #000000")));
        QVERIFY(html.contains(QStringLiteral("background-color: #D9D9D9")));
        QVERIFY(html.contains(QStringLiteral("padding: 4px 16px")));
        QVERIFY(html.contains(QStringLiteral("table.campos.beneficiario { width: 100%;")));
        QVERIFY(html.contains(QStringLiteral("<table class=\"campos beneficiario\" width=\"100%\">")));
        QVERIFY(html.contains(QStringLiteral("table.campos.fecha-pago { width: 100%; table-layout: fixed;")));
        QVERIFY(html.contains(QStringLiteral("<td class=\"linea mitad\" width=\"50%\">")));
        QVERIFY(html.contains(QStringLiteral("p.aceptacion { margin: 0 0 42px 0;")));
        QVERIFY(html.contains(QStringLiteral("table.firma-deudor { width: 100%; table-layout: fixed;")));
        QVERIFY(html.contains(QStringLiteral("<table class=\"firma-deudor\" width=\"100%\">")));
        QVERIFY(html.contains(QStringLiteral("height: 4.75in")));
        QVERIFY(html.contains(QStringLiteral("<table class=\"aval\" width=\"90%\" align=\"center\">")));
        QVERIFY(html.contains(QStringLiteral("<table class=\"firma\" width=\"45%\" align=\"center\">")));
        QVERIFY(html.contains(QStringLiteral("table.firma { width: 45%;")));
        QVERIFY(html.contains(QStringLiteral("margin: 30px 0 18px 0;")));
        QVERIFY(html.contains(QStringLiteral("margin: 28px auto 48px auto;")));
        QVERIFY(!html.contains(QRegularExpression(QStringLiteral("\\{\\{[a-z_]+\\}\\}"))));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString outputPath = directory.filePath(
            PaymentNoteTemplate::suggestedFileName(payment));
        QVERIFY2(PaymentNoteTemplate::writePdf(html, outputPath, &error), qPrintable(error));
        QVERIFY(QFileInfo(outputPath).size() > 0);
    }

    void everyMarkIsReplacedWithTheRowData()
    {
        QString error;
        const QString html = PdfMaintenanceReceiptGenerator::fillTemplate(maintenance(), &error);
        QVERIFY2(!html.isEmpty(), qPrintable(error));

        QVERIFY(html.contains(QStringLiteral("Ver. A 19 de octubre de 2026.")));
        // Las dos veces que aparece el costo, después del "$" de la plantilla.
        QCOMPARE(html.count(QStringLiteral("$12,345.50")), 2);
        QVERIFY(html.contains(QStringLiteral("mantenimiento Hojalatería y Pintura el cual")));
        // Texto plano: se escapa, y los saltos de línea se conservan.
        QVERIFY(html.contains(
            QStringLiteral("Pintura de &lt;cofre&gt; &amp; defensa<br/>Pulido")));
        for (const QString &mark : {QStringLiteral("[Fecha]"), QStringLiteral("[Costo]"),
                                    QStringLiteral("[Tipo_Mantenimiento]"),
                                    QStringLiteral("[Descripción]")})
            QVERIFY2(!html.contains(mark), qPrintable(mark));
    }

    void generatesThePdf()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("recibo.pdf"));

        PdfMaintenanceReceiptGenerator generator;
        const auto outcome = generator.generate(maintenance(), path);

        QVERIFY2(outcome.ok, qPrintable(outcome.errorMessage));
        QVERIFY(QFileInfo(path).size() > 0);
    }
};

QTEST_MAIN(TstMaintenanceReceipt)
#include "tst_maintenancereceipt.moc"
