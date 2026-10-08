// Pruebas de la regla de la autofactura: la factura se sube DESPUÉS de
// generar la solicitud de CFDI, sin perder en silencio la que ya se eligió.

#include "presentation/presenters/invoiceattachment.h"

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
};

QTEST_APPLESS_MAIN(TstInvoiceAttachment)
#include "tst_invoiceattachment.moc"
