// Pruebas de la regla de formatos del Paso 3. Es pura: recibe lo que el
// adaptador averiguó del archivo (FileFacts), así que se prueba sin disco.

#include "domain/inventory/rules/uploadformatpolicy.h"

#include <QtTest>

Q_DECLARE_METATYPE(domain::FileFacts)

using domain::FileFacts;
using domain::UploadFormatPolicy;

namespace {

FileFacts readable(const QString &fileName, const QString &contentType)
{
    FileFacts file;
    file.fileName = fileName;
    file.suffix = fileName.section(QLatin1Char('.'), -1);
    file.isReadableFile = true;
    file.opened = true;
    file.size = 1024;
    file.contentTypes << contentType;
    return file;
}

} // namespace

class TstUploadFormatPolicy : public QObject
{
    Q_OBJECT

private slots:
    void describesTheFormats()
    {
        QCOMPARE(UploadFormatPolicy::documents().describeFormats(), QStringLiteral("PDF, PNG, JPG y JPEG"));
        QCOMPARE(UploadFormatPolicy::images().dialogFilter(),
                 QStringLiteral("Imágenes (*.png *.jpg *.jpeg)"));
    }

    void acceptsAMatchingFile()
    {
        QString reason;
        QVERIFY(UploadFormatPolicy::images().accepts(readable(QStringLiteral("auto.JPG"),
                                                              QStringLiteral("image/jpeg")),
                                                     &reason));
        QVERIFY(reason.isEmpty());
        // Un PNG guardado como .jpg es de la misma familia.
        QVERIFY(UploadFormatPolicy::images().accepts(
            readable(QStringLiteral("auto.jpg"), QStringLiteral("image/png"))));
    }

    void rejectsWithTheRightReason_data()
    {
        QTest::addColumn<FileFacts>("file");
        QTest::addColumn<QString>("expected");

        FileFacts missing = readable(QStringLiteral("x.png"), QStringLiteral("image/png"));
        missing.isReadableFile = false;
        QTest::newRow("no existe") << missing << QStringLiteral("No se pudo leer 'x.png'");

        FileFacts empty = readable(QStringLiteral("x.png"), QStringLiteral("image/png"));
        empty.size = 0;
        QTest::newRow("vacío") << empty << QStringLiteral("'x.png' está vacío");

        QTest::newRow("extensión") << readable(QStringLiteral("x.heic"), QStringLiteral("image/heic"))
                                   << QStringLiteral("No se admite el formato de 'x.heic'");

        FileFacts locked = readable(QStringLiteral("x.png"), QStringLiteral("image/png"));
        locked.opened = false;
        QTest::newRow("bloqueado") << locked << QStringLiteral("No se pudo abrir 'x.png'");

        QTest::newRow("renombrado") << readable(QStringLiteral("x.png"), QStringLiteral("application/pdf"))
                                    << QStringLiteral("El contenido de 'x.png' no corresponde");
    }

    void rejectsWithTheRightReason()
    {
        QFETCH(FileFacts, file);
        QFETCH(QString, expected);
        QString reason;
        QVERIFY(!UploadFormatPolicy::images().accepts(file, &reason));
        QVERIFY2(reason.startsWith(expected), qPrintable(reason));
        // Todos los motivos repiten qué sí se puede subir.
        QVERIFY(reason.endsWith(QStringLiteral("Formatos permitidos: PNG, JPG y JPEG.")));
    }

    void checksTheSuffixWithoutTheDisk()
    {
        QVERIFY(UploadFormatPolicy::images().hasAllowedSuffix(QStringLiteral("C:/fotos/a.PNG")));
        QVERIFY(!UploadFormatPolicy::images().hasAllowedSuffix(QStringLiteral("C:/fotos/a.pdf")));
        // El punto de una carpeta no es una extensión.
        QVERIFY(!UploadFormatPolicy::images().hasAllowedSuffix(QStringLiteral("C:/fotos.png/auto")));
    }
};

QTEST_APPLESS_MAIN(TstUploadFormatPolicy)
#include "tst_uploadformatpolicy.moc"
