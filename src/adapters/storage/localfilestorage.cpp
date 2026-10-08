#include "adapters/storage/localfilestorage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>

LocalFileStorage::LocalFileStorage(QString storageRoot)
    : m_storageRoot(std::move(storageRoot))
{
}

QString LocalFileStorage::absolutePath(const QString &relativePath) const
{
    if (relativePath.isEmpty())
        return QString();
    // Una ruta ya absoluta se devuelve intacta: puede venir de un archivo que
    // el usuario acaba de elegir y todavía no se ha copiado al almacén.
    if (QDir::isAbsolutePath(relativePath))
        return relativePath;
    return QDir(m_storageRoot).filePath(relativePath);
}

QString LocalFileStorage::uniqueDestinationPath(const QString &dirPath, const QString &originalFileName) const
{
    const QFileInfo info(originalFileName);
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();
    const QString stamped = QStringLiteral("%1_%2.%3")
                                 .arg(baseName)
                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                 .arg(suffix);
    return QDir(dirPath).filePath(stamped);
}

QString LocalFileStorage::saveFile(const QString &folder, const QString &sourcePath,
                                  QString &errorMessage) const
{
    const QString dirPath = QDir(m_storageRoot).filePath(folder);
    if (!QDir().mkpath(dirPath)) {
        errorMessage = QStringLiteral("No se pudo crear la carpeta de almacenamiento: %1").arg(dirPath);
        return QString();
    }

    const QString destPath = uniqueDestinationPath(dirPath, QFileInfo(sourcePath).fileName());
    if (!QFile::copy(sourcePath, destPath)) {
        errorMessage = QStringLiteral("No se pudo copiar el archivo a %1").arg(destPath);
        return QString();
    }

    return QDir(m_storageRoot).relativeFilePath(destPath);
}

QString LocalFileStorage::saveImageCompressed(const QString &folder, const QString &sourcePath,
                                             QString &errorMessage) const
{
    QImage image(sourcePath);
    if (image.isNull()) {
        errorMessage = QStringLiteral("No se pudo leer la imagen: %1").arg(sourcePath);
        return QString();
    }

    const QString dirPath = QDir(m_storageRoot).filePath(folder);
    if (!QDir().mkpath(dirPath)) {
        errorMessage = QStringLiteral("No se pudo crear la carpeta de almacenamiento: %1").arg(dirPath);
        return QString();
    }

    // Reescalado + compresión JPG para no llenar el disco con fotos de
    // cámara/celular a resolución completa (misma técnica que ya se había
    // probado en loginwindow.cpp, ajustada a un tamaño más útil para galería).
    const QImage scaled = image.scaled(1280, 960, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const QFileInfo sourceInfo(sourcePath);
    const QString jpgName = sourceInfo.completeBaseName() + QStringLiteral(".jpg");
    const QString destPath = uniqueDestinationPath(dirPath, jpgName);

    if (!scaled.save(destPath, "JPG", 80)) {
        errorMessage = QStringLiteral("No se pudo guardar la imagen comprimida: %1").arg(destPath);
        return QString();
    }

    return QDir(m_storageRoot).relativeFilePath(destPath);
}

QString LocalFileStorage::safeFolderName(const QString &vehicleKey)
{
    QString name;
    for (const QChar ch : vehicleKey.trimmed().toUpper()) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
            name.append(ch);
        else
            name.append(QLatin1Char('_'));
    }
    return name.isEmpty() ? QStringLiteral("_sin_vin") : name;
}

application::FileStorage::Stored LocalFileStorage::store(const QString &vehicleKey,
                                                         const QString &sourcePath, Kind kind)
{
    Stored stored;
    const QString folder = QStringLiteral("vehicles/%1/%2")
                               .arg(safeFolderName(vehicleKey),
                                    kind == Kind::Image ? QStringLiteral("images")
                                                        : QStringLiteral("documents"));
    stored.relativePath = kind == Kind::Image
                              ? saveImageCompressed(folder, sourcePath, stored.errorMessage)
                              : saveFile(folder, sourcePath, stored.errorMessage);
    stored.ok = !stored.relativePath.isEmpty();
    return stored;
}

bool LocalFileStorage::remove(const QString &relativePath)
{
    if (relativePath.isEmpty() || QDir::isAbsolutePath(relativePath))
        return false; // solo se borra lo que está dentro del almacén
    return QFile::remove(QDir(m_storageRoot).filePath(relativePath));
}
