#include "presentation/views/wizard/vehiclefilesview.h"
#include "ui_vehiclefilesview.h"

#include "presentation/views/components/aspectratioimagelabel.h"
#include "presentation/views/support/formsupport.h"

#include <algorithm>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Saca de circulación un widget que acaba de salir de su layout. No se borra
// en el acto porque quien pide la reconstrucción suele ser un botón que vive
// DENTRO de ese widget (Reemplazar, quitar foto, marcar portada): destruir al
// emisor mientras su clicked() todavía se está despachando deja a Qt
// trabajando sobre un objeto borrado, y deleteLater() espera a que ese evento
// termine. Se oculta en el acto porque takeAt() lo saca del layout pero no de
// la pantalla: sin esto se seguiría viendo -- y aceptando clics, con el índice
// viejo que capturó su lambda -- hasta que llegue el borrado.
void retireWidget(QWidget *widget)
{
    widget->hide();
    widget->deleteLater();
}

// Acepta un arrastre como copia, y solo así. Esta vista nunca se queda con
// los archivos: anota sus rutas de origen y el servicio los copia al
// registrar. Por eso no sirve acceptProposedAction(): con
// Shift oprimido la acción propuesta es mover (la convención de Windows), y
// un Move aceptado le dice al Explorador que el destino ya tiene su copia; el
// Explorador termina entonces el movimiento borrando los originales --
// también los que la galería acaba de rechazar -- y las rutas anotadas se
// quedan apuntando a nada.
// Si el origen no ofrece copiar, se rechaza en vez de forzarlo:
// setDropAction() con una acción que no se ofreció no la impone, se queda con
// la propuesta, que bien puede ser ese Move.
bool acceptAsCopy(QDropEvent *event)
{
    if (!event->possibleActions().testFlag(Qt::CopyAction)) {
        event->ignore();
        return false;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    return true;
}

} // namespace

const QStringList &VehicleFilesView::documentTypes()
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

VehicleFilesView::VehicleFilesView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VehicleFilesView)
{
    // Va en código y no en el .ui porque las tres sobrecargas de arrastre de
    // abajo dependen de ella: desmarcarla en Designer apagaría la galería
    // sin que nada avisara.
    setAcceptDrops(true);

    // El .ui arma las dos tarjetas (stretch 1 y 1):
    //   - Galería: 20px de aire entre el borde del panel y las fotos (y entre
    //     filas de fotos: galleryLayout lleva spacing 20, la separación
    //     vertical entre filas). Scroll para cuando haya más fotos de las que
    //     caben en el panel. El botón "+ Agregar fotografía" va centrado y a
    //     la mitad del ancho del panel: stretch 1 / botón 2 / stretch 1.
    //   - Documentos: título, aviso de formatos (su texto llega con
    //     setUploadFormats()), aviso de error y el layout
    //     documentsLayout, donde rebuildDocuments() pone una fila por tipo.
    ui->setupUi(this);

    // Las tarjetas se llaman cardPanel en tiempo de ejecución porque así las
    // encuentra el QSS global (QFrame#cardPanel). En el .ui no pueden llevar
    // ese nombre las dos: un formulario no admite nombres repetidos (uic deja
    // la segunda como "cardPanel1" y Designer la renombra al abrirla), así
    // que ahí tienen nombre propio y se renombran aquí, antes de que la hoja
    // de estilos las pula.
    for (QFrame *card : {ui->galleryCard, ui->documentsCard})
        card->setObjectName(QStringLiteral("cardPanel"));

    // Aviso de los archivos que no se agregaron. Va bajo el título, igual que
    // el aviso del checklist del Paso 2, y arranca oculto: solo existe para
    // explicar un intento fallido. Lo mismo el de documentos.
    ui->galleryErrorLabel->setVisible(false);
    ui->documentsErrorLabel->setVisible(false);

    connect(ui->addPhotoButton, &QPushButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, QStringLiteral("Seleccionar fotografías"), QString(),
            m_imageFormats.dialogFilter);
        emit imagesChosen(paths);
    });

    rebuildDocuments();
}

VehicleFilesView::~VehicleFilesView()
{
    delete ui;
}

void VehicleFilesView::setUploadFormats(const application::UploadFormatsDto &images,
                                        const application::UploadFormatsDto &documents)
{
    m_imageFormats = images;
    m_documentFormats = documents;
    // Los formatos se anuncian antes de abrir el diálogo, no solo en el error
    // después de equivocarse. form-label (la clase que documentFormatsHint
    // lleva en el .ui) es la letra chica y gris de la hoja global; con la
    // tipografía base se leería como un renglón más del checklist.
    ui->documentFormatsHint->setText(QStringLiteral("Formatos permitidos: ") + documents.description);
}

void VehicleFilesView::addImages(const QList<presentation::ImagePreview> &images)
{
    for (const presentation::ImagePreview &preview : images) {
        GalleryImage entry;
        entry.image.path = preview.path;
        // La primera foto agregada es portada por defecto.
        entry.image.isPrimary = m_images.isEmpty();
        entry.thumbnail.loadFromData(preview.bytes);
        m_images << entry;
    }
    if (!images.isEmpty())
        rebuildGallery();
}

void VehicleFilesView::showGalleryMessage(const QString &message)
{
    ui->galleryErrorLabel->setText(message);
    ui->galleryErrorLabel->setVisible(!message.isEmpty());
}

void VehicleFilesView::showDocumentsMessage(const QString &message)
{
    ui->documentsErrorLabel->setText(message);
    ui->documentsErrorLabel->setVisible(!message.isEmpty());
}

void VehicleFilesView::showFieldErrors(const QList<domain::ValidationError> &errors)
{
    formsupport::showFieldErrors(this, errors);
}

bool VehicleFilesView::focusField(const QString &field)
{
    return formsupport::focusField(this, field);
}

void VehicleFilesView::rebuildGallery()
{
    QLayoutItem *item;
    while ((item = ui->galleryLayout->takeAt(0)) != nullptr) {
        if (QLayout *rowLayout = item->layout()) {
            // QLayout ES-A QLayoutItem: al agregarse con addLayout() no se
            // crea un wrapper aparte, así que item y rowLayout son el MISMO
            // objeto. Borrar ambos es un double-free -- solo se borra uno.
            QLayoutItem *rowItem;
            while ((rowItem = rowLayout->takeAt(0)) != nullptr) {
                // Con las tarjetas es al revés: addWidget() sí crea un
                // QWidgetItem aparte, así que borrar rowItem no toca la
                // tarjeta. Ella se retira por su lado, diferida, porque los
                // botones de quitar y de portada que llaman a esta función
                // viven dentro de ella (ver retireWidget()).
                if (QWidget *cell = rowItem->widget())
                    retireWidget(cell);
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
        const GalleryImage &entry = m_images.at(i);

        // Cada fila es su propio QHBoxLayout con un stretch antes de la
        // primera foto, entre fotos, y después de la última -- así el
        // espacio sobrante se reparte en partes iguales entre las fotos y
        // los bordes izquierdo/derecho del contenedor, en vez de estirar
        // las tarjetas mismas para llenar el ancho disponible.
        if (i % columns == 0) {
            rowLayout = new QHBoxLayout;
            rowLayout->addStretch(1);
            ui->galleryLayout->addLayout(rowLayout);
        }

        // Una sola tarjeta (mismo estilo cardPanel) envolviendo la foto y
        // los botones -- así el margen entre la foto y el borde de la
        // tarjeta queda visible, y basura/favorito quedan justo debajo,
        // alineados a los mismos bordes izquierdo/derecho de la tarjeta.
        // 20px de margen en los 4 lados; 10px separan la foto de los botones.
        auto *cell = new QFrame(ui->galleryLayout->parentWidget());
        cell->setObjectName(QStringLiteral("cardPanel"));
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(20, 20, 20, 20);
        cellLayout->setSpacing(10);

        auto *thumbnail = new AspectRatioImageLabel(cell);
        thumbnail->setSourcePixmap(entry.thumbnail);
        thumbnail->setFixedSize(360, 280);
        cellLayout->addWidget(thumbnail);

        auto *actionsLayout = new QHBoxLayout;
        auto *deleteButton = new QPushButton(QStringLiteral("🗑"), cell);
        deleteButton->setProperty("class", QStringLiteral("icon-flat"));
        connect(deleteButton, &QPushButton::clicked, this, [this, i]() {
            m_images.removeAt(i);
            if (!m_images.isEmpty() && std::none_of(m_images.cbegin(), m_images.cend(),
                                                      [](const GalleryImage &img) { return img.image.isPrimary; })) {
                m_images.first().image.isPrimary = true;
            }
            rebuildGallery();
        });

        auto *starButton = new QPushButton(entry.image.isPrimary ? QStringLiteral("★") : QStringLiteral("☆"), cell);
        starButton->setProperty("class", QStringLiteral("icon-flat"));
        connect(starButton, &QPushButton::clicked, this, [this, i]() {
            for (int j = 0; j < m_images.size(); ++j)
                m_images[j].image.isPrimary = (j == i);
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
    ui->galleryLayout->addStretch(1);
}

void VehicleFilesView::rebuildDocuments()
{
    QLayoutItem *item;
    while ((item = ui->documentsLayout->takeAt(0)) != nullptr) {
        // Los botones de subir y reemplazar, y la casilla al quitar un
        // documento, llaman a esta función desde una señal de un widget que
        // vive en una de estas filas, así que se retiran diferidas (ver
        // retireWidget()). item es el QWidgetItem que envuelve a la fila, no
        // la fila misma, y sí se puede borrar ya.
        if (QWidget *row = item->widget())
            retireWidget(row);
        delete item;
    }

    QWidget *rowParent = ui->documentsLayout->parentWidget();
    for (const QString &type : documentTypes()) {
        const bool hasFile = hasDocumentFile(type);
        const bool checked = hasFile || m_checkedDocumentTypes.contains(type);

        auto *row = new QWidget(rowParent);
        // Nombre y tipo identifican la fila sin depender del texto visible:
        // con ellos se ubica por nombre la fila de cada tipo (findChildren() +
        // la propiedad documentType) y, dentro de ella, sus controles.
        row->setObjectName(QStringLiteral("documentRow"));
        row->setProperty("documentType", type);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);

        // Primero se marca qué documento se va a subir y después se elige el
        // archivo: sin la casilla, el botón de la fila no se habilita. El
        // nombre va como texto de la propia casilla para que un clic sobre él
        // también la marque.
        auto *checkBox = new QCheckBox(type, row);
        checkBox->setObjectName(QStringLiteral("documentCheck"));
        checkBox->setChecked(checked);
        rowLayout->addWidget(checkBox, 1);

        QLineEdit *policyEdit = nullptr;
        if (type == QStringLiteral("Seguro")) {
            policyEdit = new QLineEdit(row);
            policyEdit->setObjectName(QStringLiteral("policyNumberEdit"));
            policyEdit->setPlaceholderText(QStringLiteral("No. Póliza"));
            policyEdit->setText(m_documents.value(type).documentNumber);
            policyEdit->setMaximumWidth(120);
            // La póliza es del documento Seguro: sin él marcado no hay de qué
            // capturarla.
            policyEdit->setEnabled(checked);
            connect(policyEdit, &QLineEdit::textChanged, this, [this, type](const QString &text) {
                m_documents[type].documentType = type;
                m_documents[type].documentNumber = text;
            });
            rowLayout->addWidget(policyEdit);
        }

        QPushButton *uploadButton = nullptr;
        if (hasFile) {
            auto *viewButton = new QPushButton(QStringLiteral("Ver"), row);
            viewButton->setProperty("class", QStringLiteral("secondary"));
            connect(viewButton, &QPushButton::clicked, this, [this, type]() {
                QDesktopServices::openUrl(QUrl::fromLocalFile(m_documents.value(type).path));
            });
            rowLayout->addWidget(viewButton);

            auto *replaceButton = new QPushButton(QStringLiteral("Reemplazar"), row);
            replaceButton->setProperty("class", QStringLiteral("secondary"));
            connect(replaceButton, &QPushButton::clicked, this, [this, type]() { chooseDocument(type); });
            rowLayout->addWidget(replaceButton);
        } else {
            uploadButton = new QPushButton(QStringLiteral("Subir documento"), row);
            uploadButton->setObjectName(QStringLiteral("documentUploadButton"));
            uploadButton->setProperty("class", QStringLiteral("secondary"));
            uploadButton->setEnabled(checked);
            connect(uploadButton, &QPushButton::clicked, this, [this, type]() { chooseDocument(type); });
            rowLayout->addWidget(uploadButton);
        }

        // La casilla como contexto: si la fila se retira, la conexión se va
        // con ella y la lambda nunca ve punteros a widgets ya borrados.
        connect(checkBox, &QCheckBox::toggled, checkBox,
                [this, type, checkBox, uploadButton, policyEdit](bool on) {
            if (!on && hasDocumentFile(type)) {
                // Desmarcar dice que ese documento ya no aplica, y el archivo
                // que tenía se perdería con él. Se pregunta antes para que un
                // clic de más no lo descarte en silencio.
                const auto answer = QMessageBox::question(
                    this, QStringLiteral("Quitar documento"),
                    QStringLiteral("¿Quitar %1? Se descarta el archivo que ya se había subido.").arg(type));
                if (answer != QMessageBox::Yes) {
                    // Sin bloquear la señal, volver a marcarla entraría otra vez
                    // aquí como si el usuario la hubiera marcado.
                    const QSignalBlocker blocker(checkBox);
                    checkBox->setChecked(true);
                    return;
                }
                // La entrada completa, número de póliza incluido: es del
                // documento que se acaba de quitar.
                m_documents.remove(type);
                m_checkedDocumentTypes.remove(type);
                // La fila cambia de Ver/Reemplazar a "Subir documento".
                // rebuildDocuments() la retira diferida (ver retireWidget()),
                // así que esta casilla sigue viva mientras termina su señal.
                rebuildDocuments();
                return;
            }

            if (on) {
                m_checkedDocumentTypes.insert(type);
                // Marcar un documento es empezar un intento nuevo: el aviso del
                // anterior ya no describe lo que está en pantalla.
                ui->documentsErrorLabel->setVisible(false);
            } else {
                m_checkedDocumentTypes.remove(type);
            }
            if (uploadButton)
                uploadButton->setEnabled(on);
            if (policyEdit)
                policyEdit->setEnabled(on);
        });

        ui->documentsLayout->addWidget(row);
    }
}

bool VehicleFilesView::hasDocumentFile(const QString &type) const
{
    return !m_documents.value(type).path.isEmpty();
}

void VehicleFilesView::chooseDocument(const QString &type)
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Seleccionar %1").arg(type), QString(), m_documentFormats.dialogFilter);
    // Vacía si se canceló; el presenter lo ignora y todo queda como estaba.
    emit documentChosen(type, path);
}

void VehicleFilesView::attachDocument(const QString &documentType, const QString &path)
{
    // Solo tipo y ruta, no el documento completo: el número de póliza del
    // Seguro se puede capturar antes que el archivo, y reemplazar el struct
    // entero lo borraría.
    m_documents[documentType].documentType = documentType;
    m_documents[documentType].path = path;
    rebuildDocuments();
}

bool VehicleFilesView::isImageCandidate(const QString &path) const
{
    const qsizetype separator =
        std::max(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    const qsizetype dot = path.lastIndexOf(QLatin1Char('.'));
    if (dot <= separator)
        return false;
    return m_imageFormats.extensions.contains(path.mid(dot + 1), Qt::CaseInsensitive);
}

void VehicleFilesView::dragEnterEvent(QDragEnterEvent *event)
{
    // Solo se acepta el arrastre si trae al menos una foto candidata, para que
    // el cursor avise desde antes de soltar que un PDF o un video no entran a
    // la galería. Aquí basta la extensión: abrir cada archivo en pleno
    // arrastre sería lento, y al soltar el presenter revisa cada ruta por
    // completo.
    const QList<QUrl> urls = event->mimeData()->urls();
    const bool hasCandidate = std::any_of(urls.cbegin(), urls.cend(), [this](const QUrl &url) {
        return url.isLocalFile() && isImageCandidate(url.toLocalFile());
    });
    if (hasCandidate)
        acceptAsCopy(event);
    else
        event->ignore();
}


void VehicleFilesView::dragMoveEvent(QDragMoveEvent *event)
{
    // Sin volver a buscar fotos candidatas: Qt solo manda movimientos a un
    // widget que aceptó el dragEnterEvent(), y lo que se arrastra no cambia a
    // la mitad. Lo que sí cambia es la acción propuesta, que cada movimiento
    // recalcula con las teclas de ese instante (oprimir Shift a medio camino
    // propone mover); por eso la copia se vuelve a fijar en cada uno.
    acceptAsCopy(event);
}

void VehicleFilesView::dropEvent(QDropEvent *event)
{
    // Primero se responde como copia: si el origen no la permite, se rechaza
    // el arrastre entero y no se agrega ninguna foto.
    if (!acceptAsCopy(event))
        return;

    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    emit imagesChosen(paths);
}

application::VehicleFilesDto VehicleFilesView::files() const
{
    application::VehicleFilesDto dto;
    for (const GalleryImage &entry : m_images)
        dto.images << entry.image;
    // Un tipo de documento marcado pero sin archivo no viaja.
    for (const domain::VehicleDocument &document : m_documents) {
        if (!document.path.isEmpty())
            dto.documents << document;
    }
    return dto;
}
