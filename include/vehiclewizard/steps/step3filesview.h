#ifndef STEP3FILESVIEW_H
#define STEP3FILESVIEW_H

#include "../vehicledraft.h"

#include <QList>
#include <QMap>
#include <QWidget>

class QVBoxLayout;

// Paso 3 del wizard: galería de fotos (drag-and-drop + selector, con
// marcar-portada/quitar) y checklist de documentos (Subir/Ver/Reemplazar).
// Nada se copia a storage/ ni se guarda en BD desde aquí -- solo junta rutas
// de origen en memoria; VehicleRegistrationWorker es quien copia los
// archivos al confirmar el wizard completo.
class Step3FilesView : public QWidget
{
    Q_OBJECT

public:
    explicit Step3FilesView(QWidget *parent = nullptr);

    void fillDraft(VehicleDraft &draft) const;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QWidget *buildGalleryPanel();
    QWidget *buildDocumentsPanel();
    void addImages(const QStringList &paths);
    void rebuildGallery();
    void rebuildDocuments();

    QList<PendingImage> m_images;
    QMap<QString, PendingDocument> m_documents; // key = documentType

    // Filas armadas a mano (QHBoxLayout por cada 2 fotos) en vez de
    // QGridLayout -- así el espacio sobrante se reparte como stretch antes/
    // entre/después de las tarjetas (efecto "space-around"), no como
    // estiramiento de las tarjetas mismas.
    QVBoxLayout *m_galleryLayout;
    QVBoxLayout *m_documentsLayout;

    static const QStringList &documentTypes();
};

#endif // STEP3FILESVIEW_H
