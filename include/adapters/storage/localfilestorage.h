#ifndef ADAPTERS_STORAGE_LOCALFILESTORAGE_H
#define ADAPTERS_STORAGE_LOCALFILESTORAGE_H

#include <QString>

// Copia archivos del vehículo (fotos/documentos) a
// {storageRoot}/vehicles/{vin}/{subfolder}/ y devuelve la ruta RELATIVA a
// storageRoot, que es lo que se guarda en las columnas file_path de
// PostgreSQL (nunca la ruta absoluta -- así la base es portable entre
// máquinas). No escribe nada a BD, solo maneja el disco.
class LocalFileStorage
{
public:
    explicit LocalFileStorage(QString storageRoot);

    // Copia el archivo tal cual (para documentos legales, que deben
    // conservarse byte a byte). Devuelve la ruta relativa, o QString() si
    // falla (revisar errorMessage).
    QString saveVehicleFile(const QString &vin, const QString &sourcePath,
                             const QString &subfolder, QString &errorMessage) const;

    // Igual que saveVehicleFile, pero antes reescala/recomprime la imagen
    // (no aplica a documentos). Pensado para la galería de fotos del Paso 3.
    QString saveVehicleImageCompressed(const QString &vin, const QString &sourcePath,
                                        const QString &subfolder, QString &errorMessage) const;

    // La contraparte de lectura: reconstruye la ruta absoluta a partir de la
    // relativa que quedó guardada en la base. Hace falta para mostrar las fotos
    // (la rejilla de inventario, por ejemplo), porque lo que hay en file_path
    // no se puede abrir tal cual. Devuelve QString() si la relativa viene
    // vacía, para que el llamador distinga "sin foto" de una ruta rota.
    QString absolutePath(const QString &relativePath) const;

private:
    QString uniqueDestinationPath(const QString &dirPath, const QString &originalFileName) const;

    QString m_storageRoot;
};

#endif // ADAPTERS_STORAGE_LOCALFILESTORAGE_H
