#ifndef ADAPTERS_STORAGE_LOCALFILESTORAGE_H
#define ADAPTERS_STORAGE_LOCALFILESTORAGE_H

#include "application/ports/filestorage.h"

#include <QString>

// Adaptador del puerto FileStorage sobre el disco local. Copia fotos y
// documentos a {storageRoot}/vehicles/{vin}/{documents|images}/ y devuelve la
// ruta RELATIVA a storageRoot, que es lo que se guarda en las columnas
// file_path (nunca la absoluta, para que la base sea portable entre máquinas).
// No escribe nada a la base, solo maneja el disco.
class LocalFileStorage final : public application::FileStorage
{
public:
    explicit LocalFileStorage(QString storageRoot);

    Stored store(const QString &vehicleKey, const QString &sourcePath, Kind kind) override;
    bool remove(const QString &relativePath) override;

    // Reconstruye la ruta absoluta a partir de la relativa guardada en la base
    // (p. ej. para mostrar las fotos). Una ruta ya absoluta se devuelve
    // intacta; una vacía da QString(), para distinguir "sin foto" de una ruta
    // rota.
    QString absolutePath(const QString &relativePath) const override;

    domain::FileFacts inspect(const QString &sourcePath) const override;
    QByteArray read(const QString &sourcePath) const override;
    application::TemporaryFileDto copyToTemporary(const QString &sourcePath,
                                                  const QString &fileName) override;

private:
    // El VIN se usa como nombre de carpeta: se limpia para que un "..\" o una
    // "/" no puedan escribir fuera del almacén.
    static QString safeFolderName(const QString &vehicleKey);

    QString saveFile(const QString &folder, const QString &sourcePath, QString &errorMessage) const;
    QString saveImageCompressed(const QString &folder, const QString &sourcePath,
                                QString &errorMessage) const;
    QString uniqueDestinationPath(const QString &dirPath, const QString &originalFileName) const;

    QString m_storageRoot;
};

#endif // ADAPTERS_STORAGE_LOCALFILESTORAGE_H
