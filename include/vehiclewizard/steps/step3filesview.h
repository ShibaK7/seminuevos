#ifndef STEP3FILESVIEW_H
#define STEP3FILESVIEW_H

#include "domain/vehiclebuilder.h"

#include <QList>
#include <QMap>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

// Paso 3 del wizard: galería de fotos (drag-and-drop + selector, con
// marcar-portada/quitar) y documentos. Un documento se sube eligiendo primero
// su tipo y después el archivo; el checklist de abajo muestra qué tipos ya
// tienen archivo (Ver/Reemplazar). Qué formatos admite cada panel lo decide
// UploadFormatPolicy, no esta vista.
// Nada se copia a storage/ ni se guarda en BD desde aquí -- solo junta rutas
// de origen en memoria; VehicleRegistrationWorker es quien copia los
// archivos al confirmar el wizard completo.
class Step3FilesView : public QWidget
{
    Q_OBJECT

public:
    explicit Step3FilesView(QWidget *parent = nullptr);

    void applyTo(domain::VehicleBuilder &builder) const;

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
    // Si ese tipo ya tiene archivo. Lo consultan el checklist y el botón del
    // selector, para que nunca discrepen sobre qué tipos están cubiertos.
    bool hasDocumentFile(const QString &type) const;
    // El botón del selector dice lo que su clic va a hacer: "Reemplazar
    // documento" si el tipo elegido ya tiene archivo -- el nuevo sustituiría
    // al anterior --, "Subir documento" si no lo tiene o si no hay tipo
    // elegido.
    void refreshDocumentUploadButtonText();
    // Diálogo + validación + registro de un documento del tipo dado. Lo
    // comparten el botón del selector y el "Reemplazar" de cada fila para que
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

    // Selector "primero el tipo, luego el archivo". Vive fuera de
    // m_documentsLayout porque rebuildDocuments() vacía ese layout en cada
    // carga (también al Reemplazar desde una fila), y el selector -- con el
    // tipo que el usuario ya haya elegido en él -- tiene que sobrevivir.
    QComboBox *m_documentTypeCombo = nullptr;
    QPushButton *m_documentUploadButton = nullptr;
    QLabel *m_documentsErrorLabel = nullptr;

    static const QStringList &documentTypes();
};

#endif // STEP3FILESVIEW_H
