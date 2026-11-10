#ifndef PRESENTATION_INVENTORY_REGISTRATION_STEPS_FILES_IVEHICLEFILESVIEW_H
#define PRESENTATION_INVENTORY_REGISTRATION_STEPS_FILES_IVEHICLEFILESVIEW_H

#include "application/inventory/registration/dto/registrationdtos.h"
#include "application/common/dto/uploaddtos.h"
#include "presentation/inventory/registration/istepview.h"

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
    // Solo cuenta en showFiles(), que pinta la portada guardada. Al agregar
    // fotos, la portada la decide la galería (la primera, si no había).
    bool isPrimary = false;
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

    // Pinta lo guardado (el expediente) en lugar de lo que hubiera: las
    // fotos con su portada y los documentos, con rutas que se puedan abrir.
    // Empieza de cero, sin casillas sueltas ni avisos de intentos anteriores,
    // así que files() devuelve justo lo pintado.
    virtual void showFiles(const QList<ImagePreview> &images,
                           const QList<domain::VehicleDocument> &documents) = 0;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_STEPS_FILES_IVEHICLEFILESVIEW_H
