#ifndef UPLOADFORMATPOLICY_H
#define UPLOADFORMATPOLICY_H

#include <QList>
#include <QString>
#include <QStringList>

// Qué archivos admite cada entrada del Paso 3 y, si uno no pasa, por qué.
// Es la única fuente de verdad de los formatos: el filtro del diálogo, el
// texto de ayuda, el arrastre sobre la galería y los mensajes de rechazo
// salen de aquí. Antes el único filtro era un literal en el diálogo de la
// galería y los de documentos no tenían ninguno; con cuatro sitios que
// necesitan la misma lista, tenerla en uno solo es lo que evita que el
// diálogo ofrezca algo que la validación después rechaza.
//
// Objeto de valor sin widgets ni estado mutable. Las dos políticas que existen
// son constantes de la aplicación -- documents() e images() --, y por eso el
// constructor es privado: no hay una tercera combinación que una vista tenga
// motivo para armar por su cuenta.
class UploadFormatPolicy
{
public:
    // Documentos legales: el PDF que emite la dependencia, o la foto/escaneo
    // del papel cuando solo se tiene la copia física.
    static const UploadFormatPolicy &documents();
    // Galería: solo imágenes, porque cada archivo se dibuja como miniatura y,
    // al confirmar el wizard, se recomprime a JPG (LocalFileStorageManager).
    static const UploadFormatPolicy &images();

    // Filtro para QFileDialog, p.ej. "Imágenes (*.png *.jpg *.jpeg)".
    QString dialogFilter() const;
    // Lista legible para avisos y errores, p.ej. "PDF, PNG, JPG y JPEG".
    QString describeFormats() const;

    // Revisión barata, solo por extensión y sin abrir el archivo. Sirve para
    // decidir en dragEnterEvent si un arrastre merece aceptarse; NO basta para
    // admitir un archivo, eso lo decide accepts().
    bool hasAllowedSuffix(const QString &path) const;

    // Revisión completa, en este orden: (1) que sea un archivo normal, legible
    // y con contenido; (2) que su extensión esté permitida; (3) que de verdad
    // se pueda abrir, cosa que QFileInfo no garantiza en Windows; (4) que lo
    // que hay DENTRO corresponda a esa extensión.
    //
    // El contenido se revisa porque ni el filtro ni la extensión garantizan
    // nada. El diálogo nativo de Windows deja escribir cualquier nombre, o
    // "*.*", en la caja de nombre, y el arrastre ni siquiera pasa por un
    // diálogo: el filtro por sí solo no restringe nada. Y la extensión importa
    // más de lo que parece: "Ver" abre el archivo elegido con el programa
    // asociado a su extensión, así que un PDF renombrado a .png se le
    // entregaría al visor de imágenes y no abriría. En la galería es peor: la
    // foto que no se puede decodificar no falla al elegirla sino al final,
    // cuando VehicleRegistrationWorker intenta recomprimirla al confirmar el
    // wizard, y eso aborta el registro completo.
    //
    // Si se rechaza y reason no es nulo, recibe un mensaje listo para la
    // pantalla que nombra el archivo y repite los formatos permitidos.
    bool accepts(const QString &path, QString *reason = nullptr) const;

private:
    // Extensiones que nombran el mismo tipo de contenido, junto con los tipos
    // MIME que ese contenido puede tener. El contenido se compara contra la
    // familia y no contra la extensión exacta: un PNG guardado como .jpg es un
    // descuido inofensivo (QImage y los visores deciden por contenido),
    // mientras que un PDF con nombre de imagen no se puede ver ni como una
    // cosa ni como la otra.
    struct FormatFamily
    {
        QStringList extensions; // minúsculas, sin punto, en el orden en que se muestran
        QStringList mimeTypes;
    };

    // Cada familia se define una sola vez y las dos políticas la reutilizan:
    // así lo que cuenta como "imagen" no puede desfasarse entre la galería y
    // los documentos escaneados.
    static FormatFamily pdfFamily();
    static FormatFamily imageFamily();

    UploadFormatPolicy(QString filterName, QList<FormatFamily> families);

    QStringList extensions() const;
    // La familia que reclama esa extensión, o nullptr si no está permitida.
    const FormatFamily *familyOf(const QString &suffix) const;

    QString m_filterName; // rótulo del filtro del diálogo: "Documentos" / "Imágenes"
    QList<FormatFamily> m_families;
};

#endif // UPLOADFORMATPOLICY_H
