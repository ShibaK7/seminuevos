#include "presentation/views/shell/mainwindow.h"
#include "ui_mainwindow.h"

#include "app/appconfig.h"
#include "presentation/views/components/navtabitem.h"
#include "presentation/views/components/outlinebutton.h"
#include "presentation/views/components/sidebarmenuitem.h"
#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/vehicleinventoryquery.h"
#include "adapters/storage/localfilestorage.h"
#include "presentation/views/inventory/vehicleitemlist.h"
#include "presentation/views/components/aspectratioimagelabel.h"
#include "presentation/views/wizard/vehiclewizardview.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDateEdit>
#include <QEvent>
#include <QFrame>
#include <QMargins>
#include <QLabel>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPixmapCache>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QStatusBar>

namespace {

// Se usa cuando el vehículo no tiene fotos, o cuando la que tiene ya no está en
// disco. Es preferible a dejar el hueco negro de un QPixmap nulo.
const char *kPlaceholderImage = ":/resources/images/images/background.png";

// Ancho mínimo del filtro de estado. Manda sobre el `min-width` del QSS; en
// el constructor se explica por qué.
constexpr int kStatusFilterMinWidth = 200;

// Alto del filtro de estado. Coincide a propósito con el kHeight de
// OutlineButton, para que el combo y el botón "Agregar Vehículo" queden a la
// misma altura. Los campos de fecha no lo usan: su tamaño lo da el QSS.
constexpr int kFilterControlHeight = 34;

// La lista se reconstruye entera en cada cambio de filtro. Sin caché, cada
// recarga vuelve a decodificar los JPG de 1280x960 en el hilo de la interfaz.
// La ruta relativa sirve de llave porque el nombre lleva marca de tiempo, así
// que no se repite entre archivos distintos.
QPixmap thumbnailFor(const LocalFileStorage &storage, const QString &relativePath)
{
    if (relativePath.isEmpty())
        return QPixmap(QString::fromLatin1(kPlaceholderImage));

    QPixmap cached;
    if (QPixmapCache::find(relativePath, &cached))
        return cached;

    const QPixmap loaded(storage.absolutePath(relativePath));
    // Foto borrada del disco, o una raíz de almacenamiento distinta de la que
    // la escribió: el marcador de posición es lo correcto.
    if (loaded.isNull())
        return QPixmap(QString::fromLatin1(kPlaceholderImage));

    QPixmapCache::insert(relativePath, loaded);
    return loaded;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // Las propiedades `class` de mainwindow.ui van marcadas como no
    // traducibles (notr="true"), y así tienen que quedarse si se editan en
    // Designer. Las traducibles las asigna uic al final de setupUi, en
    // retranslateUi, y para entonces la pila ya mostró inventoryPage; al
    // mostrarla, Qt pule con el QSS global esa página y todos sus hijos, y si
    // todavía no tienen `class`, las reglas [class="..."] de navBar,
    // filterCombo y dateFilter ya no les llegan. El aviso va aquí y no en el
    // .ui porque Designer borra los comentarios XML al guardar.
    ui->setupUi(this);

    ui->vehicleList->setSpacing(0);
    ui->vehicleList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Sin esto, la regla QWidget#inventoryPage del QSS no pinta nada: Qt solo
    // dibuja el fondo declarado en la hoja de estilos cuando el widget es de
    // una clase que lo pinta por su cuenta (QFrame y derivados) o cuando lleva
    // este atributo. inventoryPage es un QWidget pelado del Designer.
    ui->inventoryPage->setAttribute(Qt::WA_StyledBackground, true);

    buildSidebarMenu();
    buildNavTabs();

    // setChecked y no click(): click() emite clicked(), que dispararía una
    // recarga del inventario antes de que los filtros estén poblados.
    // autoExclusive ya se encarga de desmarcar a los hermanos.
    m_inventoryItem->setChecked(true);
    m_acquisitionTab->setChecked(true);

    populateStatusFilter();
    applyDefaultDateRange();

    // El ancho mínimo y el alto del filtro de estado se fijan aquí y mandan
    // sobre el `min-width` y el `min-height` de QComboBox[class="filterCombo"]:
    // Qt convierte esas propiedades del QSS (más padding y borde) en el tamaño
    // mínimo del widget al pulirlo, eso ya pasó dentro de setupUi (ver arriba)
    // y estas llamadas lo sobrescriben. El `min-width` sigue contando para el
    // ancho preferido; este valor solo decide hasta dónde lo encoge el layout
    // cuando falta espacio. Es fijo a propósito, no proporcional a la ventana:
    // el layout ya no le da factor de estiramiento, así que se ve igual en una
    // pantalla grande que en una chica.
    ui->estado->setMinimumWidth(kStatusFilterMinWidth);
    ui->estado->setFixedHeight(kFilterControlHeight);

    m_addVehicleButton =
        new OutlineButton(QStringLiteral("Agregar Vehículo"), QStringLiteral(":/icons/plus.png"),
                          ui->filterBar);
    ui->horizontalLayout_3->addWidget(m_addVehicleButton);

    connect(m_addVehicleButton, &QPushButton::clicked, this, &MainWindow::openVehicleWizard);

    connect(ui->estado, &QComboBox::currentIndexChanged, this, &MainWindow::reloadInventory);
    connect(ui->fechaInicio, &QDateEdit::dateChanged, this, &MainWindow::reloadInventory);
    connect(ui->fechaFin, &QDateEdit::dateChanged, this, &MainWindow::reloadInventory);

    // Las pestañas e Inventario en el menú solo cambian de página. Un
    // asistente abierto no se cierra por eso: queda vivo y oculto, con lo
    // capturado, y "Agregar Vehículo" lo retoma (ver openVehicleWizard).
    //
    // Las sub-pestañas Adquisición / Consignación todavía no filtran: por ahora
    // la lista muestra las dos ramas. Solo aseguran que se vuelva a ver el
    // inventario si el asistente estaba encima.
    connect(m_acquisitionTab, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    connect(m_consignmentTab, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });

    connect(m_inventoryItem, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    // Los otros tres módulos no existen todavía. Se avisa y se devuelve el
    // resaltado a Inventario: con autoExclusive, un manejador vacío dejaría la
    // barra lateral marcando un módulo en el que el usuario no está.
    connect(m_commercialItem, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Comercial")); });
    connect(m_financeItem, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Finanzas")); });
    connect(m_reportItem, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Reportes")); });

    reloadInventory();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showModulePending(const QString &moduleName)
{
    statusBar()->showMessage(
        QStringLiteral("El módulo de %1 todavía no está disponible.").arg(moduleName), 4000);
    m_inventoryItem->setChecked(true);
}

// ---------------------------------------------------------------------------
// Barra lateral
// ---------------------------------------------------------------------------

void MainWindow::buildSidebarMenu()
{
    // El logo se carga aquí y no con la propiedad `pixmap` del Designer:
    // AspectRatioImageLabel reescala a partir de su pixmap ORIGINAL, y
    // QLabel::setPixmap (que es lo que genera esa propiedad) no lo alimenta.
    // Declararlo en el .ui dejaba el logo en blanco al primer resize.
    ui->logoLabel->setSourcePixmap(
        QPixmap(QStringLiteral(":/resources/images/images/logo-seminuevos.jpeg")));

    // Los cuatro módulos, declarados en un solo lugar. Agregar uno nuevo es
    // una línea aquí: el aspecto y el comportamiento los aporta el componente.
    const struct
    {
        QString title;
        QString iconPath;
        SidebarMenuItem **target;
    } entries[] = {
        {QStringLiteral("Inventario"), QStringLiteral(":/icons/car.png"), &m_inventoryItem},
        {QStringLiteral("Comercial"), QStringLiteral(":/icons/dollar.png"), &m_commercialItem},
        {QStringLiteral("Finanzas"), QStringLiteral(":/icons/bank.png"), &m_financeItem},
        {QStringLiteral("Reportes"), QStringLiteral(":/icons/bar-chart.png"), &m_reportItem},
    };

    // Se insertan DESPUÉS del logo y ANTES del espaciador, que es el que
    // empuja el menú hacia arriba: agregándolos al final del layout quedarían
    // debajo de él, es decir, pegados al fondo de la barra.
    int position = 1; // 0 es el logo
    for (const auto &entry : entries) {
        auto *item = new SidebarMenuItem(entry.title, entry.iconPath, ui->sidebarFrame);
        ui->verticalLayout_2->insertWidget(position++, item);
        *entry.target = item;
    }
}

void MainWindow::buildNavTabs()
{
    // El divisor se crea ANTES que las pestañas a propósito: entre hermanos
    // que se solapan, Qt pinta primero al que se creó antes, así que el
    // subrayado de la pestaña activa queda por encima de la línea gris. Si se
    // creara después, la línea taparía el subrayado.
    m_navDivider = new QFrame(ui->navBar);
    m_navDivider->setObjectName(QStringLiteral("navDivider"));
    m_navDivider->setFrameShape(QFrame::NoFrame);
    m_navDivider->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Se insertan al inicio y en orden, antes del espaciador que la fila trae
    // del .ui: es ese espaciador el que las mantiene juntas a la izquierda en
    // vez de repartidas por todo el ancho.
    m_acquisitionTab = new NavTabItem(QStringLiteral("Adquisición"), ui->navBar);
    m_consignmentTab = new NavTabItem(QStringLiteral("Consignación"), ui->navBar);

    ui->horizontalLayout_2->insertWidget(0, m_acquisitionTab);
    ui->horizontalLayout_2->insertWidget(1, m_consignmentTab);

    ui->navBar->installEventFilter(this);
    layoutNavDivider();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        if (watched == ui->navBar)
            layoutNavDivider();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::layoutNavDivider()
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
    const int tabBottom = m_acquisitionTab->geometry().bottom() + 1;
    const int y = tabBottom - NavTabItem::kUnderlineHeight;

    const int width = ui->navBar->width() - leftInset - rightInset;
    m_navDivider->setGeometry(leftInset, y, qMax(0, width), kThickness);
}

// ---------------------------------------------------------------------------
// Filtros
// ---------------------------------------------------------------------------

void MainWindow::populateStatusFilter()
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

void MainWindow::applyDefaultDateRange()
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

VehicleInventoryFilter MainWindow::currentFilter() const
{
    VehicleInventoryFilter filter;

    const int rawStatus = ui->estado->currentData().toInt();
    if (rawStatus >= 0)
        filter.status = static_cast<domain::VehicleStatus>(rawStatus);

    filter.addedFrom = ui->fechaInicio->date();
    filter.addedTo = ui->fechaFin->date();
    return filter;
}

// ---------------------------------------------------------------------------
// Rejilla de inventario
// ---------------------------------------------------------------------------

void MainWindow::reloadInventory()
{
    // clear() destruye los renglones y, con ellos, los widgets asignados con
    // setItemWidget: la vista es dueña de ellos.
    ui->vehicleList->clear();

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();
    if (!db.isOpen()) {
        showInventoryMessage(QStringLiteral("No hay conexión con la base de datos."));
        return;
    }

    QString errorMessage;
    const QList<VehicleSummary> summaries =
        VehicleInventoryQuery::load(db, currentFilter(), &errorMessage);

    if (!errorMessage.isEmpty()) {
        showInventoryMessage(
            QStringLiteral("No se pudo leer el inventario: %1").arg(errorMessage));
        return;
    }
    if (summaries.isEmpty()) {
        showInventoryMessage(
            QStringLiteral("No hay vehículos que coincidan con los filtros seleccionados."));
        return;
    }

    const LocalFileStorage storage(AppConfig::storageRoot());
    for (const VehicleSummary &summary : summaries) {
        auto *card = new VehicleItemList(ui->vehicleList);
        card->setSummary(summary);
        card->setImage(thumbnailFor(storage, summary.coverImagePath));

        auto *item = new QListWidgetItem(ui->vehicleList);
        // Ancho cero: el renglón ocupa el ancho del viewport. Darle el de la
        // tarjeta produciría una barra horizontal de más.
        item->setSizeHint(QSize(0, card->sizeHint().height()));
        item->setData(Qt::UserRole, summary.folio);
        ui->vehicleList->setItemWidget(item, card);
    }
}

void MainWindow::showInventoryMessage(const QString &message)
{
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

void MainWindow::scrollToFolio(int folio)
{
    for (int i = 0; i < ui->vehicleList->count(); ++i) {
        QListWidgetItem *item = ui->vehicleList->item(i);
        if (item->data(Qt::UserRole).toInt() == folio) {
            ui->vehicleList->scrollToItem(item, QAbstractItemView::PositionAtTop);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Asistente de registro
// ---------------------------------------------------------------------------

void MainWindow::openVehicleWizard()
{
    // Si ya hay un asistente (el usuario se fue al inventario a media
    // captura), se vuelve a mostrar tal como quedó. Crear otro perdería lo
    // capturado y dejaría al anterior huérfano en el stack, todavía conectado
    // a esta ventana.
    if (m_wizard) {
        ui->mainContentStack->setCurrentWidget(m_wizard);
        return;
    }

    // Vista embebida, no modal: se apila sobre la página de inventario dentro
    // del mismo contenedor, en vez de abrir un diálogo aparte.
    auto *wizard = new VehicleWizardView(ui->mainContentStack);

    connect(wizard, &VehicleWizardView::returnToInventory, this, &MainWindow::closeVehicleWizard);
    connect(wizard, &VehicleWizardView::vehicleRegistered, this, &MainWindow::onVehicleRegistered);

    ui->mainContentStack->addWidget(wizard);
    ui->mainContentStack->setCurrentWidget(wizard);
    m_wizard = wizard;
}

void MainWindow::onVehicleRegistered(int folio)
{
    // Se refresca la lista de atrás en cuanto la unidad queda guardada, no al
    // cerrar: así, cuando el asistente se quite de en medio, el vehículo nuevo
    // ya está en su sitio y con el desplazamiento puesto encima.
    reloadInventory();
    scrollToFolio(folio);
}

void MainWindow::closeVehicleWizard()
{
    // El inventario se muestra antes de quitar el asistente: si se quitara
    // siendo la página visible, el stack pasaría por su cuenta a otra página
    // antes de llegar a esta.
    ui->mainContentStack->setCurrentWidget(ui->inventoryPage);
    if (!m_wizard)
        return;

    // Se quita y se destruye SIEMPRE, aunque la página visible ya fuera el
    // inventario. Pasa cuando el usuario se va al inventario mientras el
    // registro termina en segundo plano: el asistente pide cerrar estando
    // oculto. Antes, en ese caso, esta función no hacía nada y el asistente se
    // quedaba huérfano en el stack.
    //
    // removeWidget() no le quita el padre: el asistente sigue siendo hijo del
    // stack, solo deja de ser una de sus páginas y queda oculto. Así nunca
    // cuenta como ventana de nivel superior, ni por un instante (lo dice la
    // documentación de QStackedWidget y se comprobó con Qt 6.11).
    ui->mainContentStack->removeWidget(m_wizard);
    // deleteLater() y no delete: esto corre dentro de una señal del propio
    // asistente, que sigue a media ejecución. Se destruye cuando el control
    // vuelve al ciclo de eventos.
    m_wizard->deleteLater();
    // El QPointer solo se vuelve nulo cuando el objeto se destruye de verdad,
    // y eso pasa después. Se anula aquí para que "Agregar Vehículo" cree uno
    // nuevo en vez de retomar el que va de salida.
    m_wizard = nullptr;
}
