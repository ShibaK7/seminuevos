#include "presentation/inventory/inventoryview.h"
#include "ui_inventoryview.h"

#include "presentation/inventory/inventorypresenter.h"
#include "presentation/common/components/navtabitem.h"
#include "presentation/inventory/vehicleitemlist.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDateEdit>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QListWidgetItem>
#include <QMargins>
#include <QPixmap>
#include <QPixmapCache>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>

namespace {

// Se usa cuando el vehículo no tiene fotos, o cuando la que tiene ya no está en
// disco. Es preferible a dejar el hueco negro de un QPixmap nulo.
const char *kPlaceholderImage = ":/resources/images/images/background.png";

// Ancho mínimo del filtro de estado. Manda sobre el `min-width` del QSS; en
// el constructor se explica por qué.
constexpr int kStatusFilterMinWidth = 200;

// Ancho mínimo de cada filtro de fecha. El que calcula Qt reserva lugar para
// la fecha más ancha posible (276 px con este QSS), y con los dos la página no
// cabía en una laptop de 1366 px. Este alcanza para cualquier fecha real.
constexpr int kDateFilterMinWidth = 170;

// Alto del filtro de estado. Coincide a propósito con el kHeight de
// OutlineButton, para que el combo y el botón "Agregar Vehículo" queden a la
// misma altura. Los campos de fecha no lo usan: su tamaño lo da el QSS.
constexpr int kFilterControlHeight = 34;

// La lista se reconstruye entera en cada cambio de filtro, así que la
// miniatura ya decodificada se guarda en caché. La ruta relativa sirve de
// llave porque el nombre lleva marca de tiempo, así que no se repite entre
// archivos distintos. Los bytes ya llegan leídos y reducidos: la vista no abre
// archivos.
QPixmap thumbnailFor(const application::InventoryItemDto &item)
{
    if (item.coverImagePath.isEmpty() || item.coverImage.isEmpty())
        return QPixmap(QString::fromLatin1(kPlaceholderImage));

    QPixmap cached;
    if (QPixmapCache::find(item.coverImagePath, &cached))
        return cached;

    QPixmap loaded;
    loaded.loadFromData(item.coverImage);
    // Foto borrada del disco, o una raíz de almacenamiento distinta de la que
    // la escribió: el marcador de posición es lo correcto.
    if (loaded.isNull())
        return QPixmap(QString::fromLatin1(kPlaceholderImage));

    QPixmapCache::insert(item.coverImagePath, loaded);
    return loaded;
}

} // namespace

InventoryView::InventoryView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InventoryView)
{
    // Las propiedades `class` de inventoryview.ui van marcadas como no
    // traducibles (notr="true"), y así tienen que quedarse si se editan en
    // Designer: las traducibles las asigna uic al final de setupUi, y si la
    // página ya se pulió con el QSS global para entonces, las reglas
    // [class="..."] de navBar, filterCombo y dateFilter ya no le llegan. El
    // aviso va aquí y no en el .ui porque Designer borra los comentarios XML.
    ui->setupUi(this);

    ui->vehicleList->setSpacing(0);
    ui->vehicleList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Para que las tarjetas sigan al ancho de la lista (ver eventFilter()).
    ui->vehicleList->viewport()->installEventFilter(this);

    // Sin esto, la regla QWidget#inventoryPage del QSS no pinta nada: Qt solo
    // dibuja el fondo declarado en la hoja de estilos cuando el widget es de
    // una clase que lo pinta por su cuenta (QFrame y derivados) o cuando lleva
    // este atributo.
    setAttribute(Qt::WA_StyledBackground, true);

    createNavDivider();

    // La pestaña inicial va aquí y no como `checked` en el .ui: Designer ve
    // los promovidos como QPushButton no checkable, y al guardar reescribe
    // checked=true como false. Las pestañas Adquisición / Consignación
    // todavía no filtran: por ahora la lista muestra las dos ramas.
    ui->acquisitionTab->setChecked(true);

    populateStatusFilter();
    applyDefaultDateRange();

    connect(ui->addVehicleButton, &QPushButton::clicked, this, &InventoryView::addVehicleRequested);
}

InventoryView::~InventoryView()
{
    delete ui;
}

void InventoryView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_filterSized)
        return;
    m_filterSized = true;

    // El ancho mínimo y el alto del filtro de estado mandan sobre el
    // `min-width` y el `min-height` de QComboBox[class="filterCombo"]: Qt
    // convierte esas propiedades del QSS (más padding y borde) en el tamaño
    // mínimo del widget al pulirlo, y estas llamadas lo sobrescriben. Por eso
    // van aquí y no en el constructor: al mostrarse por primera vez la página
    // ya se pulió, y en el constructor todavía no, así que el QSS les ganaría.
    // El `min-width` sigue contando para el ancho preferido; este valor solo
    // decide hasta dónde lo encoge el layout cuando falta espacio. Es fijo a
    // propósito, no proporcional a la ventana: el layout ya no le da factor de
    // estiramiento, así que se ve igual en una pantalla grande que en una chica.
    ui->estado->setMinimumWidth(kStatusFilterMinWidth);
    ui->estado->setFixedHeight(kFilterControlHeight);
    for (QDateEdit *date : {ui->fechaInicio, ui->fechaFin})
        date->setMinimumWidth(kDateFilterMinWidth);

    // Lo que piden los filtros con y sin "Agregar Vehículo" en su fila. La
    // barra se declara del ancho sin el botón: si no, la página nunca podría
    // ser más angosta que la fila completa, y el botón nunca tendría por qué
    // subir. Se mide aquí por lo mismo que el filtro de estado: ya pulida la
    // hoja de estilos.
    m_oneRowFilterWidth = ui->filterBar->minimumSizeHint().width();
    moveAddVehicleButton(true);
    ui->filterBar->setMinimumWidth(ui->filterBar->minimumSizeHint().width());
    moveAddVehicleButton(false);
    placeAddVehicleButton();
}

void InventoryView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeAddVehicleButton();
}

void InventoryView::placeAddVehicleButton()
{
    // Antes del primer show todavía no se sabe cuánto piden los filtros.
    if (m_oneRowFilterWidth == 0)
        return;
    // El layout ya le dio a la barra su ancho nuevo antes de que llegue el
    // resizeEvent de la página.
    const bool toTabsRow = ui->filterBar->width() < m_oneRowFilterWidth;
    if (toTabsRow != m_addButtonInTabsRow)
        moveAddVehicleButton(toTabsRow);
}

void InventoryView::moveAddVehicleButton(bool toTabsRow)
{
    m_addButtonInTabsRow = toTabsRow;
    QBoxLayout *from = toTabsRow ? ui->horizontalLayout_3 : ui->horizontalLayout_2;
    QBoxLayout *to = toTabsRow ? ui->horizontalLayout_2 : ui->horizontalLayout_3;
    from->removeWidget(ui->addVehicleButton);
    to->addWidget(ui->addVehicleButton);
    // Cambiar de fila le cambia el padre (navBar o filterBar), y Qt lo manda
    // al final de la cadena de foco. Vuelve a donde lo puso MainWindow: justo
    // después de las pestañas (trailingFocusWidgets()).
    QWidget::setTabOrder(ui->consignmentTab, ui->addVehicleButton);
}

void InventoryView::bind(presentation::InventoryPresenter &presenter)
{
    connect(ui->estado, &QComboBox::currentIndexChanged, &presenter,
            &presentation::InventoryPresenter::reload);
    // Las fechas se teclean dígito por dígito, y cada uno cambia el control:
    // el presenter espera a que el usuario termine para leer una sola vez.
    connect(ui->fechaInicio, &QDateEdit::dateChanged, &presenter,
            &presentation::InventoryPresenter::scheduleReload);
    connect(ui->fechaFin, &QDateEdit::dateChanged, &presenter,
            &presentation::InventoryPresenter::scheduleReload);
}

QList<QWidget *> InventoryView::leadingFocusWidgets() const
{
    return {ui->estado, ui->fechaInicio, ui->fechaFin, ui->vehicleList};
}

QList<QWidget *> InventoryView::trailingFocusWidgets() const
{
    return {ui->acquisitionTab, ui->consignmentTab, ui->addVehicleButton};
}

// ---------------------------------------------------------------------------
// Pestañas
// ---------------------------------------------------------------------------

void InventoryView::createNavDivider()
{
    m_navDivider = new QFrame(ui->navBar);
    m_navDivider->setObjectName(QStringLiteral("navDivider"));
    m_navDivider->setFrameShape(QFrame::NoFrame);
    m_navDivider->setAttribute(Qt::WA_TransparentForMouseEvents);
    // Entre hermanos que se solapan, Qt pinta encima al que se creó después.
    // Las pestañas salen de setupUi, así que ya existían: sin lower(), la
    // línea gris taparía el subrayado de la pestaña activa.
    m_navDivider->lower();

    ui->navBar->installEventFilter(this);
    layoutNavDivider();
}

bool InventoryView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize && watched == ui->navBar)
        layoutNavDivider();

    // Una lista vertical calcula el ancho de sus renglones al acomodarlos y
    // no lo vuelve a calcular si solo cambia el ancho (ni con resizeMode =
    // Adjust, que solo reacciona al alto). Sin esto, al achicar la ventana o
    // al aparecer la barra vertical, las tarjetas se quedaban del ancho de
    // antes y se salían por la derecha.
    if (event->type() == QEvent::Resize && watched == ui->vehicleList->viewport()) {
        const auto *resize = static_cast<QResizeEvent *>(event);
        if (resize->size().width() != resize->oldSize().width())
            ui->vehicleList->doItemsLayout();
    }
    return QWidget::eventFilter(watched, event);
}

void InventoryView::layoutNavDivider()
{
    if (!m_navDivider)
        return;

    // Las dos sangrías se LEEN del layout de las pestañas en vez de repetirse
    // aquí: la línea arranca donde arranca la primera pestaña y termina donde
    // termina el contenido, y si alguien cambia esos márgenes en el .ui la
    // línea se mueve con ellos.
    //
    // El margen derecho es el mismo 32 px que usan la barra de filtros y la
    // superior, así que la línea acaba en la misma vertical que el botón
    // "Agregar Vehículo" y que el rol de la barra de arriba. Los contenedores
    // intermedios (verticalLayout_3, inventoryPageLayout) van en cero, que es
    // lo que permite que las tres verticales coincidan de verdad.
    const QMargins tabMargins = ui->horizontalLayout_2->contentsMargins();
    const int leftInset = tabMargins.left();
    const int rightInset = tabMargins.right();
    constexpr int kThickness = 1;

    // La altura se ancla al BOTÓN de la pestaña, no al fondo de la fila. La
    // fila puede ser un píxel más alta que sus pestañas, y anclando al fondo
    // la línea caía en el píxel INFERIOR del subrayado: se leía colgando del
    // borde de la marca de selección en lugar de cruzarla. Anclada a la
    // pestaña cae en el píxel superior de la franja, pase lo que pase con la
    // altura de la fila.
    const int tabBottom = ui->acquisitionTab->geometry().bottom() + 1;
    const int y = tabBottom - NavTabItem::kUnderlineHeight;

    const int width = ui->navBar->width() - leftInset - rightInset;
    m_navDivider->setGeometry(leftInset, y, qMax(0, width), kThickness);
}

// ---------------------------------------------------------------------------
// Filtros
// ---------------------------------------------------------------------------

void InventoryView::populateStatusFilter()
{
    // Los estados salen del dominio y no de literales en el .ui: enums.h es la
    // misma fuente que el CHECK de vehicles.status, así que no se puede filtrar
    // por un valor que la base no admite. -1 en el userData = sin filtro.
    const QSignalBlocker blocker(ui->estado);
    ui->estado->clear();
    ui->estado->addItem(QStringLiteral("Todos"), -1);
    for (const domain::VehicleStatus status : domain::allVehicleStatuses())
        ui->estado->addItem(domain::displayLabel(status), static_cast<int>(status));
    ui->estado->setCurrentIndex(0);
}

void InventoryView::applyDefaultDateRange()
{
    // Los dos controles tienen una fecha mínima fijada en el .ui, así que
    // arrancan ahí los dos. Sin estos valores por omisión el rango inicial
    // sería un solo día y la lista saldría vacía, que se lee como "la base está
    // rota" en vez de "el filtro está mal puesto".
    const QSignalBlocker blockFrom(ui->fechaInicio);
    const QSignalBlocker blockTo(ui->fechaFin);
    ui->fechaInicio->setDate(ui->fechaInicio->minimumDate());
    ui->fechaFin->setDate(QDate::currentDate());
}

application::InventoryFilterDto InventoryView::filter() const
{
    application::InventoryFilterDto filter;

    const int rawStatus = ui->estado->currentData().toInt();
    if (rawStatus >= 0)
        filter.status = static_cast<domain::VehicleStatus>(rawStatus);

    filter.addedFrom = ui->fechaInicio->date();
    filter.addedTo = ui->fechaFin->date();
    return filter;
}

// ---------------------------------------------------------------------------
// Rejilla
// ---------------------------------------------------------------------------

void InventoryView::showVehicles(const QList<application::InventoryItemDto> &vehicles)
{
    // clear() destruye los renglones y, con ellos, los widgets asignados con
    // setItemWidget: la vista es dueña de ellos.
    ui->vehicleList->clear();

    for (const application::InventoryItemDto &vehicle : vehicles) {
        auto *card = new VehicleItemList(ui->vehicleList);
        card->setItem(vehicle);
        card->setImage(thumbnailFor(vehicle));

        auto *item = new QListWidgetItem(ui->vehicleList);
        // Ancho cero: el renglón ocupa el ancho del viewport. Darle el de la
        // tarjeta produciría una barra horizontal de más. Que lo siga
        // ocupando cuando el viewport cambia de ancho lo resuelve
        // eventFilter().
        item->setSizeHint(QSize(0, card->sizeHint().height()));
        item->setData(Qt::UserRole, vehicle.folio);
        ui->vehicleList->setItemWidget(item, card);
    }
}

void InventoryView::showInventoryMessage(const QString &message)
{
    ui->vehicleList->clear();

    auto *label = new QLabel(message, ui->vehicleList);
    // Sin objectName se queda con el fondo blanco de la regla base de QWidget,
    // que sobre el gris de la página se ve como una banda blanca cruzando la
    // lista. El QSS lo deja transparente por este nombre.
    label->setObjectName(QStringLiteral("inventoryMessage"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);

    auto *item = new QListWidgetItem(ui->vehicleList);
    item->setSizeHint(QSize(0, 120));
    item->setFlags(Qt::NoItemFlags);
    ui->vehicleList->setItemWidget(item, label);
}

void InventoryView::scrollToFolio(int folio)
{
    for (int i = 0; i < ui->vehicleList->count(); ++i) {
        QListWidgetItem *item = ui->vehicleList->item(i);
        if (item->data(Qt::UserRole).toInt() == folio) {
            ui->vehicleList->scrollToItem(item, QAbstractItemView::PositionAtTop);
            return;
        }
    }
}
