#ifndef APPLICATION_PORTS_FILESTORAGE_H
#define APPLICATION_PORTS_FILESTORAGE_H

#include <QString>

namespace application {

// Puerto: el almacén de fotos y documentos de las unidades. En la base solo se
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

protected:
    FileStorage() = default;
    FileStorage(const FileStorage &) = default;
    FileStorage &operator=(const FileStorage &) = default;
};

} // namespace application

#endif // APPLICATION_PORTS_FILESTORAGE_H
