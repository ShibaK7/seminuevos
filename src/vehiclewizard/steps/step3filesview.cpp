#include "../../../include/vehiclewizard/steps/step3filesview.h"
#include "../../../include/vehiclewizard/components/aspectratioimagelabel.h"
#include "../../../include/vehiclewizard/uploadformatpolicy.h"

#include <algorithm>

#include <QComboBox>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
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

namespace {

// Cuántos rechazos de la galería se explican uno por uno. Soltar o elegir
// muchos archivos a la vez -- un Ctrl+A en la carpeta de la cámara del
// celular, por ejemplo -- puede traer decenas que no son fotos, como videos o
// HEIC, y con un renglón por cada uno el aviso crecería hasta empujar la
// galería fuera de la pantalla. Del tope en adelante solo se nombran, todos
// en un mismo párrafo.
constexpr int kMaxDetailedRejections = 3;

// Los dos textos del botón principal de documentos. Viven aquí porque se usan
// en tres sitios: al crear el botón, al medir su ancho mínimo y al cambiar de
// tipo; con el literal repetido bastaría corregir uno para que el ancho
// calculado dejara de corresponder al texto que de verdad se muestra.
const QString kUploadDocumentText = QStringLiteral("Subir documento");
const QString kReplaceDocumentText = QStringLiteral("Reemplazar documento");

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

// Texto del aviso de la galería para los archivos que no pasaron la
// validación. Cada motivo ya nombra su archivo y los formatos permitidos (lo
// redacta UploadFormatPolicy), así que aquí solo se encabeza y se acota.
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

// Acepta un arrastre como copia, y solo así. Esta vista nunca se queda con
// los archivos: anota sus rutas de origen y VehicleRegistrationWorker los
// copia al confirmar el wizard. Por eso no sirve acceptProposedAction(): con
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
    // 20px de aire entre el borde del panel y las fotos (y entre filas de
    // fotos, ver m_galleryLayout->setSpacing más abajo).
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Galería de fotos"), card);
    title->setProperty("class", QStringLiteral("h3"));
    cardLayout->addWidget(title);

    // Aviso de los archivos que no se agregaron. Va bajo el título, igual que
    // el aviso del checklist del Paso 2, y arranca oculto: solo existe para
    // explicar un intento fallido.
    m_galleryErrorLabel = new QLabel(card);
    m_galleryErrorLabel->setObjectName(QStringLiteral("galleryErrorLabel"));
    m_galleryErrorLabel->setProperty("class", QStringLiteral("error-text"));
    m_galleryErrorLabel->setWordWrap(true);
    m_galleryErrorLabel->setVisible(false);
    cardLayout->addWidget(m_galleryErrorLabel);

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
            UploadFormatPolicy::images().dialogFilter());
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

    // Selector "primero el tipo, luego el archivo". Antes cada fila del
    // checklist tenía su propio botón de subir y el tipo quedaba implícito en
    // la fila donde caía el clic; ahora es una decisión explícita, y el botón
    // no se habilita hasta que se toma.
    m_documentTypeCombo = new QComboBox(card);
    m_documentTypeCombo->setObjectName(QStringLiteral("documentTypeCombo"));
    // Arranca sin tipo: índice -1, con el placeholder a la vista. Un combo que
    // se llena sin placeholder selecciona solo su primer elemento, y un tipo
    // que nadie eligió es justo lo que este selector existe para evitar; el
    // setCurrentIndex(-1) lo deja explícito en vez de depender del orden de
    // estas líneas. El texto es el placeholder estándar del equipo, genérico
    // porque sirve para cualquier combo; qué se elige en este lo dice la
    // etiqueta que va encima.
    m_documentTypeCombo->setPlaceholderText(QStringLiteral("Seleccione una opción..."));
    for (const QString &type : documentTypes())
        m_documentTypeCombo->addItem(type, type);
    m_documentTypeCombo->setCurrentIndex(-1);

    m_documentUploadButton = new QPushButton(kReplaceDocumentText, card);
    m_documentUploadButton->setObjectName(QStringLiteral("documentUploadButton"));
    m_documentUploadButton->setProperty("class", QStringLiteral("secondary"));
    // El texto alterna entre "Subir documento" y "Reemplazar documento" según
    // el tipo elegido, y el combo de al lado ocupa el espacio que el botón
    // deja: sin un ancho fijo para el texto más largo, el combo se encogía y
    // estiraba cada vez que cambiaba la selección. Se mide con el texto largo
    // puesto y DESPUÉS de ensurePolished(): antes de pulir, el botón todavía
    // no tiene la fuente ni el padding de la hoja de estilos, y la medida
    // saldría corta -- la misma trampa que documenta ConditionChecklistRow.
    m_documentUploadButton->ensurePolished();
    m_documentUploadButton->setMinimumWidth(m_documentUploadButton->sizeHint().width());
    m_documentUploadButton->setText(kUploadDocumentText);
    m_documentUploadButton->setEnabled(false); // el combo arranca sin tipo elegido

    // Sin esta etiqueta, el placeholder genérico sería la única pista de qué se
    // elige en el combo. Va sin clase, con la tipografía base: es el nombre de
    // un campo, no letra chica como la de los formatos de abajo. setBuddy() la
    // asocia al combo para que un lector de pantalla lo anuncie con este
    // nombre.
    // Ocupa su propio renglón, justo encima de la fila del combo, y no un
    // lugar dentro de ella: ahí su ancho se sumaba al mínimo de la tarjeta, y
    // como las dos tarjetas se reparten el ancho del paso, lo que ganaba la de
    // documentos lo perdía la galería. Con la hoja de estilos de la rama de
    // integración, cuyos controles ya son más anchos, era peor: una sola foto
    // llegaba a necesitar barra de desplazamiento horizontal.
    // Y no lleva el asterisco de las etiquetas de campos obligatorios, ni debe
    // llevarlo: elegir un tipo no es requisito para guardar, solo para subir
    // un documento, y eso ya lo exige el botón, deshabilitado mientras no haya
    // un tipo elegido.
    auto *documentTypeLabel = new QLabel(QStringLiteral("Tipo de documento:"), card);
    documentTypeLabel->setObjectName(QStringLiteral("documentTypeLabel"));
    documentTypeLabel->setBuddy(m_documentTypeCombo);

    auto *selectorRow = new QHBoxLayout;
    selectorRow->addWidget(m_documentTypeCombo, 1);
    selectorRow->addWidget(m_documentUploadButton);
    cardLayout->addWidget(documentTypeLabel);
    cardLayout->addLayout(selectorRow);

    // Los formatos se anuncian antes de abrir el diálogo, no solo en el error
    // después de equivocarse. form-label es la letra chica y gris de la hoja
    // global; con la tipografía base se leería como un renglón más del
    // checklist.
    auto *formatsHint = new QLabel(
        QStringLiteral("Formatos permitidos: ") + UploadFormatPolicy::documents().describeFormats(),
        card);
    formatsHint->setObjectName(QStringLiteral("documentFormatsHint"));
    formatsHint->setProperty("class", QStringLiteral("form-label"));
    cardLayout->addWidget(formatsHint);

    m_documentsErrorLabel = new QLabel(card);
    m_documentsErrorLabel->setObjectName(QStringLiteral("documentsErrorLabel"));
    m_documentsErrorLabel->setProperty("class", QStringLiteral("error-text"));
    m_documentsErrorLabel->setWordWrap(true);
    m_documentsErrorLabel->setVisible(false);
    cardLayout->addWidget(m_documentsErrorLabel);

    connect(m_documentTypeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_documentUploadButton->setEnabled(index >= 0);
        refreshDocumentUploadButtonText();
        // Elegir un tipo es empezar un intento nuevo: el aviso del anterior ya
        // no describe lo que está en pantalla.
        if (index >= 0)
            m_documentsErrorLabel->setVisible(false);
    });

    connect(m_documentUploadButton, &QPushButton::clicked, this, [this]() {
        const QString type = m_documentTypeCombo->currentData().toString();
        if (type.isEmpty() || !uploadDocument(type))
            return;
        // Cada carga vuelve a empezar por el tipo. Si el combo se quedara con
        // el último, el siguiente clic iría directo al diálogo con ese mismo
        // tipo, y el segundo archivo (las Placas después de la Tarjeta, por
        // ejemplo) reemplazaría al primero: justo el error que pedir el tipo
        // primero quiere evitar.
        m_documentTypeCombo->setCurrentIndex(-1);
    });

    m_documentsLayout = new QVBoxLayout;
    cardLayout->addLayout(m_documentsLayout);
    cardLayout->addStretch();

    rebuildDocuments();
    return card;
}

void Step3FilesView::addImages(const QStringList &paths)
{
    // Lista vacía = se canceló el diálogo. No es un intento de carga, así que
    // tampoco borra el aviso del intento anterior.
    if (paths.isEmpty())
        return;

    const UploadFormatPolicy &policy = UploadFormatPolicy::images();
    QStringList reasons;
    QStringList rejectedNames;
    bool added = false;

    for (const QString &path : paths) {
        if (path.isEmpty())
            continue;

        // Las válidas se agregan aunque otras del mismo lote fallen: rechazar
        // el lote entero por un archivo obligaría a volver a elegir todo.
        QString reason;
        if (!policy.accepts(path, &reason)) {
            reasons << reason;
            rejectedNames << QFileInfo(path).fileName();
            continue;
        }

        domain::VehicleImage image;
        image.path = path;
        image.isPrimary = m_images.isEmpty(); // la primera foto agregada es portada por defecto
        m_images << image;
        added = true;
    }

    // El aviso describe solo el intento más reciente: uno en el que todo pasó
    // lo borra, y uno con rechazos lo reemplaza en vez de acumularse.
    m_galleryErrorLabel->setText(galleryRejectionMessage(reasons, rejectedNames));
    m_galleryErrorLabel->setVisible(!reasons.isEmpty());

    if (added)
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
        const domain::VehicleImage &image = m_images.at(i);

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
        thumbnail->setSourcePixmap(QPixmap(image.path));
        thumbnail->setFixedSize(360, 280);
        cellLayout->addWidget(thumbnail);

        auto *actionsLayout = new QHBoxLayout;
        auto *deleteButton = new QPushButton(QStringLiteral("🗑"), cell);
        deleteButton->setProperty("class", QStringLiteral("icon-flat"));
        connect(deleteButton, &QPushButton::clicked, this, [this, i]() {
            m_images.removeAt(i);
            if (!m_images.isEmpty() && std::none_of(m_images.cbegin(), m_images.cend(),
                                                      [](const domain::VehicleImage &img) { return img.isPrimary; })) {
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
        // Reemplazar llama a esta función desde el clic de un botón que vive
        // en una de estas filas, así que se retiran diferidas (ver
        // retireWidget()). item es el QWidgetItem que envuelve a la fila, no
        // la fila misma, y sí se puede borrar ya.
        if (QWidget *row = item->widget())
            retireWidget(row);
        delete item;
    }

    QWidget *rowParent = m_documentsLayout->parentWidget();
    for (const QString &type : documentTypes()) {
        const bool hasFile = hasDocumentFile(type);

        auto *row = new QWidget(rowParent);
        // Nombre y tipo identifican la fila sin depender del texto visible:
        // con ellos se ubica por nombre la fila de cada tipo (findChildren() +
        // la propiedad documentType) y, dentro de ella, documentCheck, la
        // marca que dice si ese tipo ya tiene archivo.
        row->setObjectName(QStringLiteral("documentRow"));
        row->setProperty("documentType", type);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);

        auto *checkLabel = new QLabel(hasFile ? QStringLiteral("☑") : QStringLiteral("☐"), row);
        checkLabel->setObjectName(QStringLiteral("documentCheck"));
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

        // Una fila sin archivo ya no tiene botón propio: subir empieza por el
        // selector de arriba. Reemplazar sí se queda en la fila, porque ahí la
        // fila misma ya dice de qué tipo es el archivo.
        if (hasFile) {
            auto *viewButton = new QPushButton(QStringLiteral("Ver"), row);
            viewButton->setProperty("class", QStringLiteral("secondary"));
            connect(viewButton, &QPushButton::clicked, this, [this, type]() {
                QDesktopServices::openUrl(QUrl::fromLocalFile(m_documents.value(type).path));
            });
            rowLayout->addWidget(viewButton);

            auto *replaceButton = new QPushButton(QStringLiteral("Reemplazar"), row);
            replaceButton->setProperty("class", QStringLiteral("secondary"));
            connect(replaceButton, &QPushButton::clicked, this, [this, type]() { uploadDocument(type); });
            rowLayout->addWidget(replaceButton);
        }

        m_documentsLayout->addWidget(row);
    }

    // Todo cambio de archivos pasa por esta función, así que es aquí donde se
    // pone al día el texto del botón del selector, en vez de que cada camino
    // de carga tenga que acordarse de hacerlo.
    refreshDocumentUploadButtonText();
}

bool Step3FilesView::hasDocumentFile(const QString &type) const
{
    // Por la ruta y no por contains(): capturar el número de póliza del Seguro
    // crea su entrada en m_documents aunque todavía no tenga archivo.
    return !m_documents.value(type).path.isEmpty();
}

void Step3FilesView::refreshDocumentUploadButtonText()
{
    // Sin tipo elegido, currentData() es un QVariant inválido y type queda
    // vacío: m_documents no tiene nada bajo esa clave, así que el botón
    // vuelve a "Subir documento".
    const QString type = m_documentTypeCombo->currentData().toString();
    m_documentUploadButton->setText(hasDocumentFile(type) ? kReplaceDocumentText
                                                          : kUploadDocumentText);
}

bool Step3FilesView::uploadDocument(const QString &type)
{
    const UploadFormatPolicy &policy = UploadFormatPolicy::documents();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Seleccionar %1").arg(type), QString(), policy.dialogFilter());
    if (path.isEmpty())
        return false; // se canceló: todo queda como estaba, aviso incluido

    QString reason;
    if (!policy.accepts(path, &reason)) {
        m_documentsErrorLabel->setText(reason);
        m_documentsErrorLabel->setVisible(true);
        return false;
    }

    // Solo tipo y ruta, no el documento completo: el número de póliza del
    // Seguro se puede capturar antes que el archivo, y reemplazar el struct
    // entero lo borraría.
    m_documents[type].documentType = type;
    m_documents[type].path = path;
    m_documentsErrorLabel->setVisible(false);
    rebuildDocuments();
    return true;
}

void Step3FilesView::dragEnterEvent(QDragEnterEvent *event)
{
    // Solo se acepta el arrastre si trae al menos una foto candidata, para que
    // el cursor avise desde antes de soltar que un PDF o un video no entran a
    // la galería. Aquí basta la extensión: abrir cada archivo en pleno
    // arrastre sería lento, y dropEvent() de todos modos pasa cada ruta por la
    // validación completa de addImages().
    const QList<QUrl> urls = event->mimeData()->urls();
    const bool hasCandidate = std::any_of(urls.cbegin(), urls.cend(), [](const QUrl &url) {
        return url.isLocalFile() && UploadFormatPolicy::images().hasAllowedSuffix(url.toLocalFile());
    });
    if (hasCandidate)
        acceptAsCopy(event);
    else
        event->ignore();
}

void Step3FilesView::dragMoveEvent(QDragMoveEvent *event)
{
    // Sin volver a buscar fotos candidatas: Qt solo manda movimientos a un
    // widget que aceptó el dragEnterEvent(), y lo que se arrastra no cambia a
    // la mitad. Lo que sí cambia es la acción propuesta, que cada movimiento
    // recalcula con las teclas de ese instante (oprimir Shift a medio camino
    // propone mover); por eso la copia se vuelve a fijar en cada uno.
    acceptAsCopy(event);
}

void Step3FilesView::dropEvent(QDropEvent *event)
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
    addImages(paths);
}

void Step3FilesView::applyTo(domain::VehicleBuilder &builder) const
{
    // Se limpia primero porque el builder puede venir de un intento anterior:
    // sin esto, un segundo guardado duplicaría las imágenes y chocaría contra
    // la restricción de un solo documento por tipo.
    builder.clearFiles();

    for (const domain::VehicleImage &image : m_images)
        builder.addImage(image);

    for (const domain::VehicleDocument &document : m_documents) {
        if (!document.path.isEmpty())
            builder.addDocument(document);
    }
}
