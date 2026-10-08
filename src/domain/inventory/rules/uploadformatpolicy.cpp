#include "domain/inventory/rules/uploadformatpolicy.h"

#include <algorithm>
#include <utility>

namespace domain {

namespace {

// La extensión de un nombre o una ruta, sin preguntarle nada al disco: lo que
// sigue al último punto, siempre que ese punto esté en el nombre y no en una
// carpeta ("C:/fotos.2024/auto" no tiene extensión).
QString suffixOf(const QString &fileName)
{
    const qsizetype separator =
        std::max(fileName.lastIndexOf(QLatin1Char('/')), fileName.lastIndexOf(QLatin1Char('\\')));
    const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot > separator ? fileName.mid(dot + 1) : QString();
}

} // namespace

UploadFormatPolicy::UploadFormatPolicy(QString filterName, QList<FormatFamily> families)
    : m_filterName(std::move(filterName))
    , m_families(std::move(families))
{
}

UploadFormatPolicy::FormatFamily UploadFormatPolicy::pdfFamily()
{
    return {{QStringLiteral("pdf")}, {QStringLiteral("application/pdf")}};
}

UploadFormatPolicy::FormatFamily UploadFormatPolicy::imageFamily()
{
    // JPG y JPEG son el mismo formato con dos nombres, y PNG comparte familia
    // con ellos porque quien los abre -- QImage, el visor de Windows -- se fía
    // del contenido y no del nombre.
    return {{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg")},
            {QStringLiteral("image/png"), QStringLiteral("image/jpeg")}};
}

const UploadFormatPolicy &UploadFormatPolicy::documents()
{
    static const UploadFormatPolicy policy(QStringLiteral("Documentos"),
                                           {pdfFamily(), imageFamily()});
    return policy;
}

const UploadFormatPolicy &UploadFormatPolicy::images()
{
    static const UploadFormatPolicy policy(QStringLiteral("Imágenes"), {imageFamily()});
    return policy;
}

QStringList UploadFormatPolicy::extensions() const
{
    QStringList result;
    for (const FormatFamily &family : m_families)
        result << family.extensions;
    return result;
}

QString UploadFormatPolicy::dialogFilter() const
{
    QStringList patterns;
    for (const QString &extension : extensions())
        patterns << QStringLiteral("*.") + extension;
    return QStringLiteral("%1 (%2)").arg(m_filterName, patterns.join(QLatin1Char(' ')));
}

QString UploadFormatPolicy::describeFormats() const
{
    QStringList names;
    for (const QString &extension : extensions())
        names << extension.toUpper();
    if (names.size() < 2)
        return names.value(0);

    const QString last = names.takeLast();
    return names.join(QStringLiteral(", ")) + QStringLiteral(" y ") + last;
}

const UploadFormatPolicy::FormatFamily *UploadFormatPolicy::familyOf(const QString &suffix) const
{
    // Sin distinguir mayúsculas: las cámaras suelen guardar "IMG_0001.JPG", y
    // Windows asocia ".JPG" con el mismo programa que ".jpg".
    for (const FormatFamily &family : m_families) {
        if (family.extensions.contains(suffix, Qt::CaseInsensitive))
            return &family;
    }
    return nullptr;
}

bool UploadFormatPolicy::hasAllowedSuffix(const QString &fileName) const
{
    return familyOf(suffixOf(fileName)) != nullptr;
}

bool UploadFormatPolicy::accepts(const FileFacts &file, QString *reason) const
{
    // Todos los motivos cierran igual: decir qué SÍ se puede subir es lo que
    // deja corregir sin adivinar, sea cual sea el problema.
    const auto reject = [&](const QString &problem) {
        if (reason)
            *reason = problem + QStringLiteral(" Formatos permitidos: %1.").arg(describeFormats());
        return false;
    };

    if (!file.isReadableFile) {
        return reject(QStringLiteral("No se pudo leer '%1': no existe, es una carpeta o no tienes "
                                     "permiso para abrirlo.")
                          .arg(file.fileName));
    }

    // Va antes que el contenido para dar el motivo correcto: sin bytes que
    // leer, la detección no reconoce ningún formato, y el aviso terminaría
    // hablando de un archivo renombrado cuando lo que pasa es que no tiene
    // nada.
    if (file.size == 0)
        return reject(QStringLiteral("'%1' está vacío (0 bytes).").arg(file.fileName));

    const FormatFamily *family = familyOf(file.suffix);
    if (!family)
        return reject(QStringLiteral("No se admite el formato de '%1'.").arg(file.fileName));

    // Si no se pudo abrir, la detección de contenido no vio nada, y el aviso
    // hablaría de un archivo renombrado cuando lo que pasa es otra cosa.
    if (!file.opened) {
        return reject(QStringLiteral("No se pudo abrir '%1': puede estar abierto en otro programa o "
                                     "sin permiso de lectura.")
                          .arg(file.fileName));
    }

    // contentTypes trae el tipo detectado y aquellos de los que hereda, así
    // que un subtipo que el sistema reconozca como variante del mismo formato
    // también pasa.
    const bool contentMatches =
        std::any_of(family->mimeTypes.cbegin(), family->mimeTypes.cend(),
                    [&file](const QString &mimeType) { return file.contentTypes.contains(mimeType); });
    if (!contentMatches) {
        return reject(QStringLiteral("El contenido de '%1' no corresponde a su extensión: parece "
                                     "otro tipo de archivo renombrado.")
                          .arg(file.fileName));
    }

    return true;
}

} // namespace domain
