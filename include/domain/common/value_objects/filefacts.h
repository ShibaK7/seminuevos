#ifndef DOMAIN_COMMON_VALUE_OBJECTS_FILEFACTS_H
#define DOMAIN_COMMON_VALUE_OBJECTS_FILEFACTS_H

#include <QString>
#include <QStringList>

namespace domain {

// Lo que se sabe de un archivo que el usuario quiere subir, ya averiguado por
// quien sí puede tocar el disco (el adaptador de almacenamiento). Con esto la
// regla de formatos decide sin abrir nada, y se prueba sin archivos.
struct FileFacts
{
    QString fileName;            // sin carpeta, para los mensajes
    QString suffix;              // extensión sin punto, tal como viene
    bool isReadableFile = false; // existe, es un archivo (no carpeta) y hay permiso
    qint64 size = 0;
    // Se pudo abrir para leer. En Windows isReadableFile no lo garantiza: un
    // archivo que otro programa tiene bloqueado también "existe y es legible".
    bool opened = false;
    // Tipo MIME detectado por CONTENIDO (nunca por el nombre), con sus alias y
    // los tipos de los que hereda. Vacío si no se pudo leer.
    QStringList contentTypes;
};

} // namespace domain

#endif // DOMAIN_COMMON_VALUE_OBJECTS_FILEFACTS_H
