#include "presentation/views/shell/mainwindow.h"
#include "ui_mainwindow.h"

#include "presentation/views/components/navtabitem.h"
#include "presentation/presenters/inventorypresenter.h"
#include "presentation/views/inventory/vehicleitemlist.h"
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
// que no se repite entre archivos distintos. Los bytes ya llegan leídos: la
// vista no abre archivos.
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

    createNavDivider();

    // La selección inicial va aquí y no como `checked` en el .ui: Designer ve
    // los promovidos como QPushButton no checkable, y al guardar reescribe
    // checked=true como false. setChecked y no click(): click() emite
    // clicked(), que recargaría el inventario antes de poblar los filtros.
    ui->inventoryItem->setChecked(true);
    ui->acquisitionTab->setChecked(true);

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

    connect(ui->addVehicleButton, &QPushButton::clicked, this, &MainWindow::openVehicleWizard);

    // Las pestañas e Inventario en el menú solo cambian de página. Un
    // asistente abierto no se cierra por eso: queda vivo y oculto, con lo
    // capturado, y "Agregar Vehículo" lo retoma (ver openVehicleWizard).
    //
    // Las sub-pestañas Adquisición / Consignación todavía no filtran: por ahora
    // la lista muestra las dos ramas. Solo aseguran que se vuelva a ver el
    // inventario si el asistente estaba encima.
    connect(ui->acquisitionTab, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    connect(ui->consignmentTab, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });

    connect(ui->inventoryItem, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    // Los otros tres módulos no existen todavía. Se avisa y se devuelve el
    // resaltado a Inventario: con autoExclusive, un manejador vacío dejaría la
    // barra lateral marcando un módulo en el que el usuario no está.
    connect(ui->commercialItem, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Comercial")); });
    connect(ui->financeItem, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Finanzas")); });
    connect(ui->reportItem, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Reportes")); });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showModulePending(const QString &moduleName)
{
    statusBar()->showMessage(
        QStringLiteral("El módulo de %1 todavía no está disponible.").arg(moduleName), 4000);
    ui->inventoryItem->setChecked(true);
}

// ---------------------------------------------------------------------------
// Pestañas de contenido
// ---------------------------------------------------------------------------

void MainWindow::createNavDivider()
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
    const int tabBottom = ui->acquisitionTab->geometry().bottom() + 1;
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

application::InventoryFilterDto MainWindow::filter() const
{
    application::InventoryFilterDto filter;

    const int rawStatus = ui->estado->currentData().toInt();
    if (rawStatus >= 0)
        filter.status = static_cast<domain::VehicleStatus>(rawStatus);

    filter.addedFrom = ui->fechaInicio->date();
    filter.addedTo = ui->fechaFin->date();
    return filter;
}

void MainWindow::bindInventory(presentation::InventoryPresenter &presenter)
{
    m_inventory = &presenter;
    connect(ui->estado, &QComboBox::currentIndexChanged, &presenter,
            &presentation::InventoryPresenter::reload);
    // Las fechas se teclean dígito por dígito, y cada uno cambia el control:
    // el presenter espera a que el usuario termine para leer una sola vez.
    connect(ui->fechaInicio, &QDateEdit::dateChanged, &presenter,
            &presentation::InventoryPresenter::scheduleReload);
    connect(ui->fechaFin, &QDateEdit::dateChanged, &presenter,
            &presentation::InventoryPresenter::scheduleReload);
}

void MainWindow::setSession(const application::SessionDto &session)
{
    // El rol tal como lo guarda la base ("administrador"), con mayúscula
    // inicial para la barra.
    QString role = session.role;
    if (!role.isEmpty())
        role[0] = role.at(0).toUpper();
    ui->userRoleLabel->setText(role);
    ui->userRoleLabel->setToolTip(session.displayName);
}

// ---------------------------------------------------------------------------
// Rejilla de inventario
// ---------------------------------------------------------------------------

void MainWindow::showVehicles(const QList<application::InventoryItemDto> &vehicles)
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
        // tarjeta produciría una barra horizontal de más.
        item->setSizeHint(QSize(0, card->sizeHint().height()));
        item->setData(Qt::UserRole, vehicle.folio);
        ui->vehicleList->setItemWidget(item, card);
    }
}

void MainWindow::showInventoryMessage(const QString &message)
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
    if (!m_wizardFactory)
        return;
    VehicleWizardView *wizard = m_wizardFactory(ui->mainContentStack);

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
    if (m_inventory)
        m_inventory->vehicleRegistered(folio);
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

void MainWindow::setWizardFactory(WizardFactory factory)
{
    m_wizardFactory = std::move(factory);
}
