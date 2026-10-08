#include "presentation/presenters/vehiclefilespresenter.h"

#include "application/services/vehicleregistrationservice.h"
#include "presentation/presenters/ivehiclefilesview.h"

namespace presentation {

namespace {

// Cuántos rechazos de la galería se explican uno por uno. Soltar o elegir
// muchos archivos a la vez -- un Ctrl+A en la carpeta de la cámara del
// celular, por ejemplo -- puede traer decenas que no son fotos, como videos o
// HEIC, y con un renglón por cada uno el aviso crecería hasta empujar la
// galería fuera de la pantalla. Del tope en adelante solo se nombran, todos
// en un mismo párrafo.
constexpr int kMaxDetailedRejections = 3;

// Cada motivo ya nombra su archivo y los formatos permitidos (lo redacta la
// regla de formatos), así que aquí solo se encabeza y se acota.
QString galleryRejectionMessage(const QStringList &reasons, const QStringList &fileNames)
{
    if (reasons.isEmpty())
        return QString();

    QStringList lines;
    lines << (reasons.size() == 1
                  ? QStringLiteral("No se agregó este archivo:")
                  : QStringLiteral("No se agregaron estos %1 archivos:").arg(reasons.size()));
    lines << reasons.mid(0, kMaxDetailedRejections);
    if (reasons.size() > kMaxDetailedRejections) {
        lines << QStringLiteral("Tampoco se agregaron: %1.")
                     .arg(fileNames.mid(kMaxDetailedRejections).join(QStringLiteral(", ")));
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

VehicleFilesPresenter::VehicleFilesPresenter(IVehicleFilesView &view,
                                             const application::VehicleRegistrationService &service)
    : WizardStepPresenter(view)
    , m_view(view)
    , m_service(service)
{
}

QString VehicleFilesPresenter::title() const
{
    return QStringLiteral("Archivos");
}

bool VehicleFilesPresenter::ownsField(const QString &field) const
{
    return field.startsWith(QStringLiteral("images"))
           || field.startsWith(QStringLiteral("documents"));
}

void VehicleFilesPresenter::start()
{
    m_view.setUploadFormats(m_service.uploadFormats(application::UploadKind::Image),
                            m_service.uploadFormats(application::UploadKind::Document));
}

void VehicleFilesPresenter::onImagesChosen(const QStringList &paths)
{
    // Lista vacía = se canceló el diálogo. No es un intento de carga, así que
    // tampoco borra el aviso del intento anterior.
    if (paths.isEmpty())
        return;

    QList<ImagePreview> accepted;
    QStringList reasons;
    QStringList rejectedNames;
    for (const QString &path : paths) {
        if (path.isEmpty())
            continue;
        const application::UploadCheckDto check =
            m_service.checkUpload(path, application::UploadKind::Image);
        if (!check.accepted) {
            reasons << check.reason;
            rejectedNames << check.fileName;
            continue;
        }
        accepted << ImagePreview{path, m_service.filePreview(path)};
    }

    // El aviso describe solo el intento más reciente: uno en el que todo pasó
    // lo borra, y uno con rechazos lo reemplaza en vez de acumularse.
    m_view.showGalleryMessage(galleryRejectionMessage(reasons, rejectedNames));
    if (!accepted.isEmpty())
        m_view.addImages(accepted);
}

void VehicleFilesPresenter::onDocumentChosen(const QString &documentType, const QString &path)
{
    if (path.isEmpty())
        return; // se canceló: todo queda como estaba, aviso incluido

    const application::UploadCheckDto check =
        m_service.checkUpload(path, application::UploadKind::Document);
    if (!check.accepted) {
        m_view.showDocumentsMessage(check.reason);
        return;
    }
    m_view.showDocumentsMessage(QString());
    m_view.attachDocument(documentType, path);
}

application::VehicleFilesDto VehicleFilesPresenter::dto() const
{
    return m_view.files();
}

domain::ValidationResult VehicleFilesPresenter::collectErrors() const
{
    // El Paso 3 todavía no tiene reglas propias: las fotos y los documentos
    // son opcionales. Lo que sí puede fallar ahí, como un documento repetido,
    // lo reporta el servicio al guardar, y ownsField() lo trae a este paso.
    return domain::ValidationResult();
}

} // namespace presentation
