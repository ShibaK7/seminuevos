#include "vehiclewizard/uploadformatpolicy.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <algorithm>
#include <utility>

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

bool UploadFormatPolicy::hasAllowedSuffix(const QString &path) const
{
    return familyOf(QFileInfo(path).suffix()) != nullptr;
}

bool UploadFormatPolicy::accepts(const QString &path, QString *reason) const
{
    const QFileInfo info(path);
    const QString fileName = info.fileName();

    // Todos los motivos cierran igual: decir qué SÍ se puede subir es lo que
    // deja corregir sin adivinar, sea cual sea el problema.
    const auto reject = [&](const QString &problem) {
        if (reason)
            *reason = problem + QStringLiteral(" Formatos permitidos: %1.").arg(describeFormats());
        return false;
    };

    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return reject(QStringLiteral("No se pudo leer '%1': no existe, es una carpeta o no tienes "
                                     "permiso para abrirlo.")
                          .arg(fileName));
    }

    // Va antes que el contenido para dar el motivo correcto: sin bytes que
    // leer, la detección no reconoce ningún formato, y el aviso terminaría
    // hablando de un archivo renombrado cuando lo que pasa es que no tiene
    // nada.
    if (info.size() == 0)
        return reject(QStringLiteral("'%1' está vacío (0 bytes).").arg(fileName));

    const FormatFamily *family = familyOf(info.suffix());
    if (!family)
        return reject(QStringLiteral("No se admite el formato de '%1'.").arg(fileName));

    // Se abre aquí, antes de revisar el contenido, porque nada de lo de arriba
    // garantiza que se pueda: en Windows isReadable() solo confirma que el
    // archivo existe (Qt no consulta los permisos de NTFS si no se le pide), y
    // un archivo que otro programa tiene bloqueado también existe. Si la
    // apertura se dejara a la detección, su falla no se vería: devolvería
    // application/octet-stream y el aviso hablaría de un archivo renombrado.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return reject(QStringLiteral("No se pudo abrir '%1': puede estar abierto en otro programa o "
                                     "sin permiso de lectura.")
                          .arg(fileName));
    }

    // mimeTypeForData() solo ve los bytes, nunca el nombre, y eso es a
    // propósito: lo que se quiere saber es qué hay dentro, y por nombre la
    // respuesta sería la misma extensión que ya se revisó arriba. Lee del
    // archivo ya abierto, así que se abre una sola vez y se revisa justo lo
    // que se abrió. inherits() y no una comparación exacta, para no rechazar
    // un subtipo que el sistema reconozca como variante del mismo formato.
    const QMimeType content = QMimeDatabase().mimeTypeForData(&file);
    const bool contentMatches =
        std::any_of(family->mimeTypes.cbegin(), family->mimeTypes.cend(),
                    [&content](const QString &mimeType) { return content.inherits(mimeType); });
    if (!contentMatches) {
        return reject(QStringLiteral("El contenido de '%1' no corresponde a su extensión: parece "
                                     "otro tipo de archivo renombrado.")
                          .arg(fileName));
    }

    return true;
}
