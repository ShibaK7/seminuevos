#include "../../../include/vehiclewizard/steps/step3filesview.h"
#include "../../../include/vehiclewizard/components/aspectratioimagelabel.h"

#include <algorithm>

#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

const QStringList &Step3FilesView::documentTypes()
{
    static const QStringList types = {
        QStringLiteral("Tarjeta de circulación"),
        QStringLiteral("Placas"),
        QStringLiteral("Tenencias"),
        QStringLiteral("Verificaciones"),
        QStringLiteral("Reporte Sin Robo"),
        QStringLiteral("Estancia Legal en el País"),
        QStringLiteral("Manual de Usuario"),
        QStringLiteral("Seguro"),
    };
    return types;
}

Step3FilesView::Step3FilesView(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(buildGalleryPanel(), 1);
    layout->addWidget(buildDocumentsPanel(), 1);
}

QWidget *Step3FilesView::buildGalleryPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    auto *cardLayout = new QVBoxLayout(card);
    // 20px de aire entre el borde del panel y las fotos (y entre fotos,
    // ver m_galleryGrid->setSpacing más abajo).
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Galería de fotos"), card);
    title->setProperty("class", QStringLiteral("h3"));
    cardLayout->addWidget(title);

    // Scroll para cuando haya más fotos de las que caben en el panel.
    auto *scrollArea = new QScrollArea(card);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollContent = new QWidget(scrollArea);
    m_galleryLayout = new QVBoxLayout(scrollContent);
    m_galleryLayout->setContentsMargins(0, 0, 0, 0);
    m_galleryLayout->setSpacing(20); // separación vertical entre filas de fotos

    scrollArea->setWidget(scrollContent);
    cardLayout->addWidget(scrollArea, 1);

    auto *addButton = new QPushButton(QStringLiteral("+ Agregar fotografía"), card);
    addButton->setProperty("class", QStringLiteral("secondary"));
    // Centrado y a la mitad del ancho del panel: stretch 1 / botón 2 / stretch 1.
    addButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, QStringLiteral("Seleccionar fotografías"), QString(),
            QStringLiteral("Imágenes (*.png *.jpg *.jpeg)"));
        addImages(paths);
    });

    auto *addButtonRow = new QHBoxLayout;
    addButtonRow->addStretch(1);
    addButtonRow->addWidget(addButton, 2);
    addButtonRow->addStretch(1);
    cardLayout->addLayout(addButtonRow);

    return card;
}

QWidget *Step3FilesView::buildDocumentsPanel()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    auto *cardLayout = new QVBoxLayout(card);

    auto *title = new QLabel(QStringLiteral("Documentos"), card);
    title->setProperty("class", QStringLiteral("h3"));
    cardLayout->addWidget(title);

    m_documentsLayout = new QVBoxLayout;
    cardLayout->addLayout(m_documentsLayout);
    cardLayout->addStretch();

    rebuildDocuments();
    return card;
}

void Step3FilesView::addImages(const QStringList &paths)
{
    for (const QString &path : paths) {
        if (path.isEmpty())
            continue;
        PendingImage image;
        image.sourcePath = path;
        image.isPrimary = m_images.isEmpty(); // la primera foto agregada es portada por defecto
        m_images << image;
    }
    rebuildGallery();
}

void Step3FilesView::rebuildGallery()
{
    QLayoutItem *item;
    while ((item = m_galleryLayout->takeAt(0)) != nullptr) {
        if (QLayout *rowLayout = item->layout()) {
            // QLayout ES-A QLayoutItem: al agregarse con addLayout() no se
            // crea un wrapper aparte, así que item y rowLayout son el MISMO
            // objeto. Borrar ambos es un double-free -- solo se borra uno.
            QLayoutItem *rowItem;
            while ((rowItem = rowLayout->takeAt(0)) != nullptr) {
                delete rowItem->widget();
                delete rowItem;
            }
            delete rowLayout;
        } else {
            delete item; // QSpacerItem u otro item que no envuelve un layout
        }
    }

    const int columns = 2;
    QHBoxLayout *rowLayout = nullptr;

    for (int i = 0; i < m_images.size(); ++i) {
        const PendingImage &image = m_images.at(i);

        // Cada fila es su propio QHBoxLayout con un stretch antes de la
        // primera foto, entre fotos, y después de la última -- así el
        // espacio sobrante se reparte en partes iguales entre las fotos y
        // los bordes izquierdo/derecho del contenedor, en vez de estirar
        // las tarjetas mismas para llenar el ancho disponible.
        if (i % columns == 0) {
            rowLayout = new QHBoxLayout;
            rowLayout->addStretch(1);
            m_galleryLayout->addLayout(rowLayout);
        }

        // Una sola tarjeta (mismo estilo cardPanel) envolviendo la foto y
        // los botones -- así el margen entre la foto y el borde de la
        // tarjeta queda visible, y basura/favorito quedan justo debajo,
        // alineados a los mismos bordes izquierdo/derecho de la tarjeta.
        // 20px de margen en los 4 lados; 10px separan la foto de los botones.
        auto *cell = new QFrame(m_galleryLayout->parentWidget());
        cell->setObjectName(QStringLiteral("cardPanel"));
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(20, 20, 20, 20);
        cellLayout->setSpacing(10);

        auto *thumbnail = new AspectRatioImageLabel(cell);
        thumbnail->setSourcePixmap(QPixmap(image.sourcePath));
        thumbnail->setFixedSize(360, 280);
        cellLayout->addWidget(thumbnail);

        auto *actionsLayout = new QHBoxLayout;
        auto *deleteButton = new QPushButton(QStringLiteral("🗑"), cell);
        deleteButton->setProperty("class", QStringLiteral("icon-flat"));
        connect(deleteButton, &QPushButton::clicked, this, [this, i]() {
            m_images.removeAt(i);
            if (!m_images.isEmpty() && std::none_of(m_images.cbegin(), m_images.cend(),
                                                      [](const PendingImage &img) { return img.isPrimary; })) {
                m_images.first().isPrimary = true;
            }
            rebuildGallery();
        });

        auto *starButton = new QPushButton(image.isPrimary ? QStringLiteral("★") : QStringLiteral("☆"), cell);
        starButton->setProperty("class", QStringLiteral("icon-flat"));
        connect(starButton, &QPushButton::clicked, this, [this, i]() {
            for (int j = 0; j < m_images.size(); ++j)
                m_images[j].isPrimary = (j == i);
            rebuildGallery();
        });

        actionsLayout->addWidget(deleteButton, 0, Qt::AlignLeft);
        actionsLayout->addStretch();
        actionsLayout->addWidget(starButton, 0, Qt::AlignRight);
        cellLayout->addLayout(actionsLayout);

        rowLayout->addWidget(cell);
        rowLayout->addStretch(1);
    }

    // Absorbe el espacio vertical sobrante para que las filas no se estiren.
    m_galleryLayout->addStretch(1);
}

void Step3FilesView::rebuildDocuments()
{
    QLayoutItem *item;
    while ((item = m_documentsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (const QString &type : documentTypes()) {
        const bool hasFile = m_documents.contains(type) && !m_documents.value(type).sourcePath.isEmpty();

        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);

        auto *checkLabel = new QLabel(hasFile ? QStringLiteral("☑") : QStringLiteral("☐"), row);
        rowLayout->addWidget(checkLabel);

        auto *nameLabel = new QLabel(type, row);
        rowLayout->addWidget(nameLabel, 1);

        if (type == QStringLiteral("Seguro")) {
            auto *policyEdit = new QLineEdit(row);
            policyEdit->setPlaceholderText(QStringLiteral("No. Póliza"));
            policyEdit->setText(m_documents.value(type).documentNumber);
            policyEdit->setMaximumWidth(120);
            connect(policyEdit, &QLineEdit::textChanged, this, [this, type](const QString &text) {
                m_documents[type].documentType = type;
                m_documents[type].documentNumber = text;
            });
            rowLayout->addWidget(policyEdit);
        }

        if (hasFile) {
            auto *viewButton = new QPushButton(QStringLiteral("Ver"), row);
            viewButton->setProperty("class", QStringLiteral("secondary"));
            connect(viewButton, &QPushButton::clicked, this, [this, type]() {
                QDesktopServices::openUrl(QUrl::fromLocalFile(m_documents.value(type).sourcePath));
            });
            rowLayout->addWidget(viewButton);

            auto *replaceButton = new QPushButton(QStringLiteral("Reemplazar"), row);
            replaceButton->setProperty("class", QStringLiteral("secondary"));
            connect(replaceButton, &QPushButton::clicked, this, [this, type]() {
                const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Seleccionar %1").arg(type));
                if (path.isEmpty())
                    return;
                m_documents[type].documentType = type;
                m_documents[type].sourcePath = path;
                rebuildDocuments();
            });
            rowLayout->addWidget(replaceButton);
        } else {
            auto *uploadButton = new QPushButton(QStringLiteral("Subir documento"), row);
            uploadButton->setProperty("class", QStringLiteral("secondary"));
            connect(uploadButton, &QPushButton::clicked, this, [this, type]() {
                const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Seleccionar %1").arg(type));
                if (path.isEmpty())
                    return;
                m_documents[type].documentType = type;
                m_documents[type].sourcePath = path;
                rebuildDocuments();
            });
            rowLayout->addWidget(uploadButton);
        }

        m_documentsLayout->addWidget(row);
    }
}

void Step3FilesView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void Step3FilesView::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    addImages(paths);
    event->acceptProposedAction();
}

void Step3FilesView::fillDraft(VehicleDraft &draft) const
{
    draft.images = m_images;

    draft.documents.clear();
    for (const PendingDocument &document : m_documents) {
        if (!document.sourcePath.isEmpty())
            draft.documents << document;
    }
}
