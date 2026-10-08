#ifndef APPLICATION_PORTS_FILESTORAGE_H
#define APPLICATION_PORTS_FILESTORAGE_H

#include "application/dto/uploaddtos.h"
#include "domain/value_objects/filefacts.h"

#include <QByteArray>
#include <QString>

namespace application {

// Puerto: todo el acceso a disco de la aplicación. Ninguna vista ni presenter
// abre archivos: piden lo que necesitan aquí, a través del servicio.
//
// El almacén guarda fotos y documentos de las unidades. En la base solo se
// guardan rutas RELATIVAS al almacén, así que la base es portable entre
// máquinas; absolutePath() las resuelve.
class FileStorage
{
public:
    enum class Kind {
        Document, // se copia tal cual (documentos legales)
        Image,    // se recomprime para la galería
    };

    struct Stored
    {
        bool ok = false;
        QString relativePath;
        QString errorMessage;
    };

    virtual ~FileStorage() = default;

    // Copia `sourcePath` al almacén de la unidad `vehicleKey` (su VIN).
    virtual Stored store(const QString &vehicleKey, const QString &sourcePath, Kind kind) = 0;

    // Borra un archivo ya guardado. Es la compensación cuando la base rechaza
    // la unidad después de haber copiado sus archivos.
    virtual bool remove(const QString &relativePath) = 0;

    virtual QString absolutePath(const QString &relativePath) const = 0;

    // --- Archivos de origen (los que el usuario elige, fuera del almacén) ---

    // Lo que hace falta para decidir si se admite un archivo: si existe, su
    // tamaño, si se puede abrir y qué hay dentro.
    virtual domain::FileFacts inspect(const QString &sourcePath) const = 0;

    // Los bytes de un archivo, para su vista previa. Vacío si no se pudo leer.
    virtual QByteArray read(const QString &sourcePath) const = 0;

    // Copia `sourcePath` (también puede ser un recurso ":/...") a la carpeta
    // temporal del sistema con el nombre `fileName`.
    virtual TemporaryFileDto copyToTemporary(const QString &sourcePath, const QString &fileName) = 0;

protected:
    FileStorage() = default;
    FileStorage(const FileStorage &) = default;
    FileStorage &operator=(const FileStorage &) = default;
};

} // namespace application

#endif // APPLICATION_PORTS_FILESTORAGE_H
