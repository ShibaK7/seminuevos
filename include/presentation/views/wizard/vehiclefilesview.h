#ifndef PRESENTATION_VIEWS_WIZARD_VEHICLEFILESVIEW_H
#define PRESENTATION_VIEWS_WIZARD_VEHICLEFILESVIEW_H

#include "application/dto/registrationdtos.h"
#include "application/dto/uploaddtos.h"
#include "presentation/presenters/ivehiclefilesview.h"

#include <QList>
#include <QMap>
#include <QPixmap>
#include <QSet>
#include <QWidget>

class QLabel;
class QVBoxLayout;

// Paso 3 del wizard: galería de fotos (drag-and-drop + selector, con
// marcar-portada/quitar) y documentos. Cada documento del checklist se marca
// primero con su casilla, y solo entonces se habilita el botón para subir su
// archivo; los que ya tienen archivo ofrecen Ver/Reemplazar.
//
// Vista pasiva (IVehicleFilesView): no abre ningún archivo. Avisa qué rutas
// eligió el usuario (imagesChosen, documentChosen) y VehicleFilesPresenter
// decide cuáles se admiten y le pasa las miniaturas ya leídas. Nada se copia
// al almacén desde aquí: el servicio copia los archivos al registrar.
class VehicleFilesView : public QWidget, public presentation::IVehicleFilesView
{
    Q_OBJECT

public:
    explicit VehicleFilesView(QWidget *parent = nullptr);

    // --- IVehicleFilesView ---
    application::VehicleFilesDto files() const override;
    void setUploadFormats(const application::UploadFormatsDto &images,
                          const application::UploadFormatsDto &documents) override;
    void addImages(const QList<presentation::ImagePreview> &images) override;
    void showGalleryMessage(const QString &message) override;
    void attachDocument(const QString &documentType, const QString &path) override;
    void showDocumentsMessage(const QString &message) override;
    void showFieldErrors(const QList<domain::ValidationError> &errors) override;
    bool focusField(const QString &field) override;

signals:
    void imagesChosen(const QStringList &paths);
    void documentChosen(const QString &documentType, const QString &path);

protected:
    // Los tres aceptan un arrastre solo como copia, nunca como Move: la vista
    // no se queda con los archivos, solo anota sus rutas, y un Move aceptado
    // haría que el Explorador borrara los originales.
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    // Una foto de la galería con su miniatura ya decodificada.
    struct GalleryImage
    {
        domain::VehicleImage image;
        QPixmap thumbnail;
    };

    QWidget *buildGalleryPanel();
    QWidget *buildDocumentsPanel();
    void rebuildGallery();
    void rebuildDocuments();
    // Si ese tipo ya tiene archivo. Por la ruta y no por contains(): capturar
    // el número de póliza del Seguro crea su entrada aunque no tenga archivo.
    bool hasDocumentFile(const QString &type) const;
    // Diálogo de un documento del tipo dado. Lo comparten el "Subir documento"
    // y el "Reemplazar" de cada fila para que no exista un camino de carga que
    // se salte la revisión de formato.
    void chooseDocument(const QString &type);
    // Revisión barata por extensión, para el cursor de un arrastre. La
    // revisión de verdad la hace el presenter al soltar.
    bool isImageCandidate(const QString &path) const;

    QList<GalleryImage> m_images;
    QMap<QString, domain::VehicleDocument> m_documents; // key = documentType

    application::UploadFormatsDto m_imageFormats;
    application::UploadFormatsDto m_documentFormats;

    // Filas armadas a mano (QHBoxLayout por cada 2 fotos) en vez de
    // QGridLayout -- así el espacio sobrante se reparte como stretch antes/
    // entre/después de las tarjetas (efecto "space-around"), no como
    // estiramiento de las tarjetas mismas.
    QVBoxLayout *m_galleryLayout;
    QVBoxLayout *m_documentsLayout;

    QLabel *m_documentFormatsHint = nullptr;
    QLabel *m_galleryErrorLabel = nullptr;
    QLabel *m_documentsErrorLabel = nullptr;

    // Tipos cuya casilla se marcó. Se guardan aparte porque rebuildDocuments()
    // vuelve a crear las filas en cada carga, y sin esto una casilla recién
    // marcada se desmarcaría al subir el archivo de otra fila. Una fila con
    // archivo sale marcada de todos modos: solo pudo subirse con la casilla
    // puesta.
    QSet<QString> m_checkedDocumentTypes;

    static const QStringList &documentTypes();
};

#endif // PRESENTATION_VIEWS_WIZARD_VEHICLEFILESVIEW_H
