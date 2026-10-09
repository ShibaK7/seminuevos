// Pruebas del adaptador de disco sobre una carpeta temporal: miniaturas,
// lo que averigua de un archivo para la regla de formatos, copias al almacén
// y su compensación, y la copia a la carpeta temporal.

#include "adapters/storage/localfilestorage.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

class TstLocalFileStorage : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString writeImage(const QString &name, int width, int height)
    {
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(Qt::darkBlue);
        const QString path = m_dir.filePath(name);
        image.save(path);
        return path;
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void thumbnailFitsTheBoxKeepingTheAspect()
    {
        LocalFileStorage storage(m_dir.filePath(QStringLiteral("almacen")));
        const QString path = writeImage(QStringLiteral("grande.png"), 2000, 1000);

        QImage thumbnail;
        QVERIFY(thumbnail.loadFromData(storage.readThumbnail(path, 480, 360)));
        QCOMPARE(thumbnail.size(), QSize(480, 240));
    }

    void missingImageGivesAnEmptyThumbnail()
    {
        LocalFileStorage storage(m_dir.filePath(QStringLiteral("almacen")));
        QVERIFY(storage.readThumbnail(m_dir.filePath(QStringLiteral("no-existe.jpg")), 480, 360).isEmpty());
    }

    // El tipo se detecta por contenido: un PDF renombrado a .png no pasa por
    // imagen.
    void inspectDetectsTheRealContent()
    {
        LocalFileStorage storage(m_dir.filePath(QStringLiteral("almacen")));
        const QString png = writeImage(QStringLiteral("foto.png"), 10, 10);
        domain::FileFacts facts = storage.inspect(png);
        QVERIFY(facts.isReadableFile);
        QVERIFY(facts.opened);
        QCOMPARE(facts.suffix, QStringLiteral("png"));
        QVERIFY(facts.contentTypes.contains(QStringLiteral("image/png")));

        const QString fake = m_dir.filePath(QStringLiteral("disfrazado.png"));
        QFile file(fake);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("%PDF-1.4\n%prueba\n");
        file.close();
        facts = storage.inspect(fake);
        QVERIFY(!facts.contentTypes.contains(QStringLiteral("image/png")));
        QVERIFY(facts.contentTypes.contains(QStringLiteral("application/pdf")));
    }

    void storeAndRemoveStayInsideTheStorage()
    {
        LocalFileStorage storage(m_dir.filePath(QStringLiteral("almacen")));
        const QString source = writeImage(QStringLiteral("auto.png"), 1600, 1200);

        const auto stored = storage.store(QStringLiteral("../VIN 1"), source,
                                          application::FileStorage::Kind::Image);
        QVERIFY2(stored.ok, qPrintable(stored.errorMessage));
        // El VIN se sanea: ni "../" ni espacios salen del almacén.
        QVERIFY(stored.relativePath.startsWith(QStringLiteral("vehicles/___VIN_1/images/")));
        QVERIFY(QFile::exists(storage.absolutePath(stored.relativePath)));

        QVERIFY(storage.remove(stored.relativePath));
        QVERIFY(!QFile::exists(storage.absolutePath(stored.relativePath)));
        // Solo borra rutas relativas al almacén.
        QVERIFY(!storage.remove(source));
        QVERIFY(QFile::exists(source));
    }

    void copyToTemporaryWritesTheWholeFile()
    {
        LocalFileStorage storage(m_dir.filePath(QStringLiteral("almacen")));
        const QString source = writeImage(QStringLiteral("plantilla.png"), 20, 20);
        const application::TemporaryFileDto copy =
            storage.copyToTemporary(source, QStringLiteral("seminuevos-prueba.png"));
        QVERIFY2(copy.ok, qPrintable(copy.errorMessage));
        QCOMPARE(storage.read(copy.path), storage.read(source));
        QFile::remove(copy.path);

        const application::TemporaryFileDto missing =
            storage.copyToTemporary(m_dir.filePath(QStringLiteral("nada.html")), QStringLiteral("x.html"));
        QVERIFY(!missing.ok);
        QVERIFY(!missing.errorMessage.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstLocalFileStorage)
#include "tst_localfilestorage.moc"
