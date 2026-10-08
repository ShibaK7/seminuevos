#ifndef PRESENTATION_PRESENTERS_IVEHICLEFILESVIEW_H
#define PRESENTATION_PRESENTERS_IVEHICLEFILESVIEW_H

#include "application/dto/registrationdtos.h"
#include "application/dto/uploaddtos.h"
#include "presentation/presenters/istepview.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace presentation {

// Una foto ya aceptada, con los bytes de su miniatura: la vista la pinta sin
// abrir el archivo.
struct ImagePreview
{
    QString path;
    QByteArray bytes;
};

// Paso 3 del asistente: galería y documentos. La vista junta rutas de origen;
// el presenter decide cuáles se admiten y le pasa las miniaturas.
class IVehicleFilesView : public IStepView
{
public:
    virtual application::VehicleFilesDto files() const = 0;

    virtual void setUploadFormats(const application::UploadFormatsDto &images,
                                  const application::UploadFormatsDto &documents) = 0;

    virtual void addImages(const QList<ImagePreview> &images) = 0;
    // Aviso de la galería sobre el último intento. Vacío = ocultarlo.
    virtual void showGalleryMessage(const QString &message) = 0;

    virtual void attachDocument(const QString &documentType, const QString &path) = 0;
    // Aviso de los documentos sobre el último intento. Vacío = ocultarlo.
    virtual void showDocumentsMessage(const QString &message) = 0;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_IVEHICLEFILESVIEW_H
