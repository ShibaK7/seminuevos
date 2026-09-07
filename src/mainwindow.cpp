#include "../include/mainwindow.h"
#include "../ui/ui_mainwindow.h"

#include "../include/app/appconfig.h"
#include "../include/db/connectionpool.h"
#include "../include/db/vehicleinventoryquery.h"
#include "../include/storage/localfilestoragemanager.h"
#include "../include/vehicleitemlist.h"
#include "../include/vehiclewizard/vehiclewizardview.h"

#include <QComboBox>
#include <QDateEdit>
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

// La lista se reconstruye entera en cada cambio de filtro. Sin caché, cada
// recarga vuelve a decodificar los JPG de 1280x960 en el hilo de la interfaz.
// La ruta relativa sirve de llave porque el nombre lleva marca de tiempo, así
// que no se repite entre archivos distintos.
QPixmap thumbnailFor(const LocalFileStorageManager &storage, const QString &relativePath)
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
    ui->setupUi(this);
    ui->vehicleList->setSpacing(0);
    ui->vehicleList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // setChecked y no click(): click() emite clicked(), que dispararía una
    // recarga del inventario antes de que los filtros estén poblados.
    // autoExclusive ya se encarga de desmarcar a los hermanos.
    ui->inventoryButton->setChecked(true);
    ui->acquisitionButton->setChecked(true);

    populateStatusFilter();
    applyDefaultDateRange();

    connect(ui->addVehicleButton, &QPushButton::clicked, this, &MainWindow::openVehicleWizard);

    connect(ui->estado, &QComboBox::currentIndexChanged, this, &MainWindow::reloadInventory);
    connect(ui->fechaInicio, &QDateEdit::dateChanged, this, &MainWindow::reloadInventory);
    connect(ui->fechaFin, &QDateEdit::dateChanged, this, &MainWindow::reloadInventory);

    // Las sub-pestañas Adquisición / Consignación todavía no filtran: por ahora
    // la lista muestra las dos ramas. Solo aseguran que se vuelva a ver el
    // inventario si el asistente estaba encima.
    connect(ui->acquisitionButton, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    connect(ui->consignmentButton, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });

    connect(ui->inventoryButton, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    // Los otros tres módulos no existen todavía. Se avisa y se devuelve el
    // resaltado a Inventario: con autoExclusive, un manejador vacío dejaría la
    // barra lateral marcando un módulo en el que el usuario no está.
    connect(ui->commercialButton, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Comercial")); });
    connect(ui->financeButton, &QPushButton::clicked, this,
            [this] { showModulePending(QStringLiteral("Finanzas")); });
    connect(ui->reportButton, &QPushButton::clicked, this,
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
    ui->inventoryButton->setChecked(true);
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

    const LocalFileStorageManager storage(AppConfig::storageRoot());
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
    // Vista embebida, no modal: se apila sobre la página de inventario dentro
    // del mismo contenedor, en vez de abrir un diálogo aparte.
    auto *wizard = new VehicleWizardView(ui->mainContentStack);

    connect(wizard, &VehicleWizardView::returnToInventory, this, &MainWindow::closeVehicleWizard);
    connect(wizard, &VehicleWizardView::vehicleRegistered, this, &MainWindow::onVehicleRegistered);

    ui->mainContentStack->addWidget(wizard);
    ui->mainContentStack->setCurrentWidget(wizard);
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
    QWidget *current = ui->mainContentStack->currentWidget();
    if (current == ui->inventoryPage)
        return;

    ui->mainContentStack->setCurrentWidget(ui->inventoryPage);
    // Se destruye sin llamar a removeWidget(): esa función reparenta el widget
    // a nullptr, con lo que por un instante deja de tener padre y cuenta como
    // ventana de nivel superior. Al destruirse justo después, Qt puede
    // interpretar que se cerró la última ventana y terminar la aplicación
    // entera. Al destruirlo directo, el contenedor lo suelta solo y el widget
    // nunca deja de tener padre.
    current->deleteLater();
}
