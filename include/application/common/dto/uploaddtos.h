#ifndef APPLICATION_COMMON_DTO_UPLOADDTOS_H
#define APPLICATION_COMMON_DTO_UPLOADDTOS_H

#include <QString>
#include <QStringList>

namespace application {

// A qué entrada del Paso 3 va un archivo: cada una admite formatos distintos.
enum class UploadKind {
    Image,    // galería
    Document, // documentos legales
};

// Lo que la pantalla necesita saber de los formatos de una entrada para
// ofrecerlos, sin conocer la regla que los decide.
struct UploadFormatsDto
{
    QString dialogFilter;   // "Imágenes (*.png *.jpg *.jpeg)"
    QString description;    // "PNG, JPG y JPEG"
    QStringList extensions; // minúsculas y sin punto, para revisar un arrastre
};

// Veredicto sobre un archivo que el usuario eligió.
struct UploadCheckDto
{
    bool accepted = false;
    QString fileName; // sin carpeta, para nombrarlo en el aviso
    QString reason;   // listo para la pantalla; vacío si se aceptó
};

// Un archivo escrito en la carpeta temporal del sistema para abrirlo con el
// programa que corresponda (el formulario de solicitud de CFDI).
struct TemporaryFileDto
{
    bool ok = false;
    QString path;
    QString errorMessage;
};

} // namespace application

#endif // APPLICATION_COMMON_DTO_UPLOADDTOS_H
