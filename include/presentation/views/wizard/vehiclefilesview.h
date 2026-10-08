#ifndef PRESENTATION_VIEWS_WIZARD_VEHICLEFILESVIEW_H
#define PRESENTATION_VIEWS_WIZARD_VEHICLEFILESVIEW_H

#include "application/dto/registrationdtos.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>

class QLabel;
class QVBoxLayout;

// Paso 3 del wizard: galería de fotos (drag-and-drop + selector, con
// marcar-portada/quitar) y documentos. Cada documento del checklist se marca
// primero con su casilla, y solo entonces se habilita el botón para subir su
// archivo; los que ya tienen archivo ofrecen Ver/Reemplazar. Qué formatos
// admite cada panel lo decide UploadFormatPolicy, no esta vista.
// Nada se copia a storage/ ni se guarda en BD desde aquí -- solo junta rutas
// de origen en memoria; VehicleRegistrationWorker es quien copia los
// archivos al confirmar el wizard completo.
class VehicleFilesView : public QWidget
{
    Q_OBJECT

public:
    explicit VehicleFilesView(QWidget *parent = nullptr);

    application::VehicleFilesDto files() const;

protected:
    // Los tres aceptan un arrastre solo como copia, nunca como Move: la vista
    // no se queda con los archivos, solo anota sus rutas, y un Move aceptado
    // haría que el Explorador borrara los originales.
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QWidget *buildGalleryPanel();
    QWidget *buildDocumentsPanel();
    // Única puerta de entrada de fotos, vengan del diálogo o de un arrastre:
    // valida cada ruta y avisa en el panel cuáles no se agregaron.
    void addImages(const QStringList &paths);
    void rebuildGallery();
    void rebuildDocuments();
    // Si ese tipo ya tiene archivo. Por la ruta y no por contains(): capturar
    // el número de póliza del Seguro crea su entrada aunque no tenga archivo.
    bool hasDocumentFile(const QString &type) const;
    // Diálogo + validación + registro de un documento del tipo dado. Lo
    // comparten el "Subir documento" y el "Reemplazar" de cada fila para que
    // no exista un camino de carga que se salte la validación de formato.
    // Devuelve true solo si el tipo quedó con un archivo nuevo: cancelar o
    // elegir uno inválido no cambian lo que ya estaba cargado.
    bool uploadDocument(const QString &type);

    QList<domain::VehicleImage> m_images;
    QMap<QString, domain::VehicleDocument> m_documents; // key = documentType

    // Filas armadas a mano (QHBoxLayout por cada 2 fotos) en vez de
    // QGridLayout -- así el espacio sobrante se reparte como stretch antes/
    // entre/después de las tarjetas (efecto "space-around"), no como
    // estiramiento de las tarjetas mismas.
    QVBoxLayout *m_galleryLayout;
    QVBoxLayout *m_documentsLayout;

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
