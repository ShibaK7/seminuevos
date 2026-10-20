// El recibo de un mantenimiento (#47) con su plantilla real: cada marca
// [Fecha], [Costo], [Tipo_Mantenimiento] y [Descripción] se sustituye con los
// datos del renglón, escapados, y el PDF se genera con las imágenes del
// recurso.

#include "adapters/contract/pdfmaintenancereceiptgenerator.h"

#include <QFileInfo>
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
