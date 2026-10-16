// Pruebas de la regla de la autofactura: la factura se sube DESPUÉS de
// generar la solicitud de CFDI, sin perder en silencio la que ya se eligió.

#include "presentation/inventory/acquisition/invoiceattachment.h"

#include <QtTest>

using presentation::InvoiceAttachment;

class TstInvoiceAttachment : public QObject
{
    Q_OBJECT

private slots:
    void startsEmptyAndFree()
    {
        const InvoiceAttachment attachment;
        QVERIFY(attachment.canUpload());
        QVERIFY(!attachment.showsCfdiButton());
        QCOMPARE(attachment.label(), QStringLiteral("Sin archivo"));
        QVERIFY(attachment.uploadToolTip().isEmpty());
    }

    void autofacturaLocksUploadUntilTheRequest()
    {
        InvoiceAttachment attachment;
        attachment.setAutofactura(true);
        QVERIFY(!attachment.canUpload());
        QVERIFY(attachment.showsCfdiButton());
        QVERIFY(!attachment.uploadToolTip().isEmpty());

        attachment.markCfdiRequestGenerated();
        QVERIFY(attachment.canUpload());
        QVERIFY(attachment.uploadToolTip().isEmpty());
    }

    // Pasar a Autofactura sin solicitud aparta la factura; volver la recupera.
    void switchingToAutofacturaSuspendsAndBackRestores()
    {
        InvoiceAttachment attachment;
        attachment.attach(QStringLiteral("C:\\facturas\\f1.pdf"));
        QCOMPARE(attachment.label(), QStringLiteral("f1.pdf"));

        attachment.setAutofactura(true);
        QVERIFY(attachment.attachedPath().isEmpty());
        QVERIFY(attachment.hasSuspendedFile());
        QCOMPARE(attachment.label(), QStringLiteral("Factura retirada"));
        QVERIFY(!attachment.labelToolTip().isEmpty());

        attachment.setAutofactura(false);
        QCOMPARE(attachment.attachedPath(), QStringLiteral("C:\\facturas\\f1.pdf"));
        QCOMPARE(attachment.label(), QStringLiteral("f1.pdf"));
        QVERIFY(attachment.labelToolTip().isEmpty());
    }

    // Generar la solicitud descarta la factura apartada: en autofactura solo
    // vale la que se sube después.
    void cfdiRequestDiscardsTheSuspendedFile()
    {
        InvoiceAttachment attachment;
        attachment.attach(QStringLiteral("/f1.pdf"));
        attachment.setAutofactura(true);
        attachment.markCfdiRequestGenerated();
        QVERIFY(!attachment.hasSuspendedFile());
        QCOMPARE(attachment.label(), QStringLiteral("Sin archivo"));

        attachment.setAutofactura(false);
        QVERIFY(attachment.attachedPath().isEmpty());
    }

    // Una factura subida después de la solicitud sí vale, y sobrevive a ir y
    // volver entre tipos y a generar la solicitud otra vez.
    void fileUploadedAfterTheRequestStays()
    {
        InvoiceAttachment attachment;
        attachment.setAutofactura(true);
        attachment.markCfdiRequestGenerated();
        attachment.attach(QStringLiteral("/f2.pdf"));

        attachment.setAutofactura(false);
        attachment.setAutofactura(true);
        QCOMPARE(attachment.attachedPath(), QStringLiteral("/f2.pdf"));

        attachment.markCfdiRequestGenerated();
        QCOMPARE(attachment.label(), QStringLiteral("f2.pdf"));
    }

    // El expediente parte de la factura guardada y olvida lo anterior,
    // apartada incluida.
    void restoreStartsFromTheStoredInvoice()
    {
        InvoiceAttachment attachment;
        attachment.attach(QStringLiteral("/vieja.pdf"));
        attachment.setAutofactura(true);
        QVERIFY(attachment.hasSuspendedFile());

        attachment.restore(QStringLiteral("C:/almacen/f1.pdf"), false);
        QVERIFY(!attachment.hasSuspendedFile());
        QCOMPARE(attachment.attachedPath(), QStringLiteral("C:/almacen/f1.pdf"));
        QCOMPARE(attachment.label(), QStringLiteral("f1.pdf"));
        QVERIFY(attachment.canUpload());
        QVERIFY(!attachment.showsCfdiButton());

        // Pasar a Autofactura la aparta, igual que en el alta.
        attachment.setAutofactura(true);
        QVERIFY(attachment.attachedPath().isEmpty());
        QVERIFY(attachment.hasSuspendedFile());
    }

    // Una autofactura guardada con su factura ya pasó por la solicitud: no
    // se vuelve a pedir, e ir y volver por Facturado no aparta la factura.
    void restoredAutofacturaWithInvoiceKeepsIt()
    {
        InvoiceAttachment attachment;
        attachment.restore(QStringLiteral("C:/almacen/f1.pdf"), true);
        QVERIFY(attachment.cfdiRequestGenerated());
        QVERIFY(attachment.canUpload());
        QVERIFY(attachment.showsCfdiButton());

        attachment.setAutofactura(false);
        attachment.setAutofactura(true);
        QCOMPARE(attachment.attachedPath(), QStringLiteral("C:/almacen/f1.pdf"));
    }

    // Sin factura no hay cómo saber si la solicitud se generó: se pide otra
    // vez antes de subir. Tampoco cuenta una solicitud de antes.
    void restoredAutofacturaWithoutInvoiceAsksForTheRequest()
    {
        InvoiceAttachment attachment;
        attachment.markCfdiRequestGenerated();
        attachment.restore(QString(), true);
        QVERIFY(!attachment.cfdiRequestGenerated());
        QVERIFY(!attachment.canUpload());
        QCOMPARE(attachment.label(), QStringLiteral("Sin archivo"));
    }
};

QTEST_APPLESS_MAIN(TstInvoiceAttachment)
#include "tst_invoiceattachment.moc"
