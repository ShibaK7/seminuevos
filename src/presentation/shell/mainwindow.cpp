#include "presentation/shell/mainwindow.h"
#include "src/presentation/shell/ui_mainwindow.h"

#include "presentation/inventory/inventorypresenter.h"
#include "presentation/inventory/inventoryview.h"
#include "presentation/commercial/commercialmainview.h"
#include "presentation/commercial/commercialpresenter.h"
#include "presentation/commercial/saledetails/SaleDetails.h"
#include "presentation/inventory/record/vehiclerecordview.h"
#include "presentation/inventory/registration/vehiclewizardview.h"
#include "presentation/commercial/registration/salewizardview.h"

#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // La selección inicial va aquí y no como `checked` en el .ui: Designer ve
    // los promovidos como QPushButton no checkable, y al guardar reescribe
    // checked=true como false.
    ui->inventoryItem->setChecked(true);

    // El orden de tabulación cruza dos formularios (este y el de la página del
    // inventario), así que ninguno de los dos .ui lo puede declarar completo:
    // filtros y lista, después el menú lateral, y al final las pestañas y
    // "Agregar Vehículo". Es también lo que deja el foco inicial en el filtro
    // de estado.
    QList<QWidget *> focusChain = ui->inventoryPage->leadingFocusWidgets();
    focusChain << ui->inventoryItem << ui->commercialItem << ui->financeItem << ui->reportItem;
    focusChain << ui->inventoryPage->trailingFocusWidgets();
    for (qsizetype i = 0; i + 1 < focusChain.size(); ++i)
        QWidget::setTabOrder(focusChain.at(i), focusChain.at(i + 1));

    connect(ui->inventoryPage, &InventoryView::addVehicleRequested, this,
            &MainWindow::openVehicleWizard);
    // Encolada a propósito: abrir otro expediente puede preguntar si se
    // descartan cambios, y ese diálogo no debe correr dentro del clic de una
    // tarjeta que la rejilla podría destruir mientras está abierto (si el
    // inventario se recarga en ese momento).
    connect(ui->inventoryPage, &InventoryView::vehicleDetailsRequested, this,
            &MainWindow::openVehicleRecord, Qt::QueuedConnection);
    connect(ui->commercial_main_view, &CommercialMainView::newSaleRequested,
            this, &MainWindow::openSaleWizard, Qt::QueuedConnection);
    connect(ui->commercial_main_view, &CommercialMainView::saleDetailsRequested,
            this, &MainWindow::openSaleDetails, Qt::QueuedConnection);

    // Inventario en el menú solo cambia de página. Un asistente abierto no se
    // cierra por eso: queda vivo y oculto, con lo capturado, y "Agregar
    // Vehículo" lo retoma (ver openVehicleWizard).
    connect(ui->inventoryItem, &QPushButton::clicked, this,
            [this] { ui->mainContentStack->setCurrentWidget(ui->inventoryPage); });
    // Los otros tres módulos no existen todavía. Se avisa y se devuelve el
    // resaltado a Inventario: con autoExclusive, un manejador vacío dejaría la
    // barra lateral marcando un módulo en el que el usuario no está.
    connect(ui->commercialItem, &QPushButton::clicked, this, [this] {
        ui->commercialItem->setChecked(true);
        ui->mainContentStack->setCurrentWidget(ui->commercial_main_view);
    });
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

InventoryView &MainWindow::inventoryView()
{
    return *ui->inventoryPage;
}

SaleWizardView &MainWindow::saleWizardView()
{
    return *ui->commercialPage;
}

CommercialMainView &MainWindow::commercialView()
{
    return *ui->commercial_main_view;
}

void MainWindow::bindInventory(presentation::InventoryPresenter &presenter)
{
    m_inventory = &presenter;
    ui->inventoryPage->bind(presenter);
}

void MainWindow::bindCommercial(presentation::CommercialPresenter &presenter)
{
    ui->commercial_main_view->bind(presenter);
}

void MainWindow::setSaleDetailsFactory(SaleDetailsFactory factory)
{
    m_saleDetailsFactory = std::move(factory);
}

void MainWindow::setSession(const application::SessionDto &session)
{
    ui->userRoleLabel->setText(domain::displayLabel(session.role));
    ui->userRoleLabel->setToolTip(session.displayName);
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
    if (!m_wizard)
        return;

    // Se quita y se destruye SIEMPRE, aunque la página visible ya fuera otra.
    // Pasa cuando el usuario se va al inventario, o a un expediente, mientras
    // el registro termina en segundo plano: el asistente pide cerrar estando
    // oculto. Antes, en ese caso, esta función no hacía nada y el asistente se
    // quedaba huérfano en el stack.
    dropPage(m_wizard);
    // El QPointer solo se vuelve nulo cuando el objeto se destruye de verdad,
    // y eso pasa después. Se anula aquí para que "Agregar Vehículo" cree uno
    // nuevo en vez de retomar el que va de salida.
    m_wizard = nullptr;
}

void MainWindow::setWizardFactory(WizardFactory factory)
{
    m_wizardFactory = std::move(factory);
}

void MainWindow::openSaleWizard()
{
    ui->mainContentStack->setCurrentWidget(ui->commercialPage);
    ui->commercialPage->setFocus();
}

void MainWindow::openSaleDetails(int folio)
{
    if (m_saleDetails)
        closeSaleDetails();
    if (!m_saleDetailsFactory)
        return;

    SaleDetails *details = m_saleDetailsFactory(ui->mainContentStack, folio);
    connect(details, &SaleDetails::returnToCommercial, this, [this, details] {
        if (details == m_saleDetails)
            closeSaleDetails();
    });
    ui->mainContentStack->addWidget(details);
    ui->mainContentStack->setCurrentWidget(details);
    m_saleDetails = details;
}

void MainWindow::closeSaleDetails()
{
    if (!m_saleDetails)
        return;
    if (ui->mainContentStack->currentWidget() == m_saleDetails)
        ui->mainContentStack->setCurrentWidget(ui->commercial_main_view);
    ui->mainContentStack->removeWidget(m_saleDetails);
    m_saleDetails->deleteLater();
    m_saleDetails = nullptr;
}

// ---------------------------------------------------------------------------
// Expediente
// ---------------------------------------------------------------------------

void MainWindow::openVehicleRecord(int folio)
{
    if (m_record) {
        if (folio == m_recordFolio) {
            // El mismo folio: se vuelve a mostrar tal como quedó y se relee,
            // por si la unidad cambió desde que se abrió.
            ui->mainContentStack->setCurrentWidget(m_record);
            m_record->refresh();
            return;
        }
        // Otro folio: el abierto decide si se puede cerrar (pregunta si tiene
        // cambios sin guardar). Si el usuario se queda con ellos, se le
        // muestra para que siga donde iba.
        if (!m_record->confirmLeave()) {
            ui->mainContentStack->setCurrentWidget(m_record);
            return;
        }
        closeVehicleRecord();
    }

    if (!m_recordFactory)
        return;
    VehicleRecordView *record = m_recordFactory(ui->mainContentStack, folio);

    // La lambda solo cierra si ese expediente sigue siendo el abierto: uno que
    // ya va de salida no debe cerrar al que lo reemplazó. La conexión muere
    // con él, así que nunca ve un puntero colgando.
    connect(record, &VehicleRecordView::returnToInventory, this, [this, record] {
        if (record == m_record)
            closeVehicleRecord();
    });
    connect(record, &VehicleRecordView::vehicleChanged, this, &MainWindow::onVehicleChanged);

    ui->mainContentStack->addWidget(record);
    ui->mainContentStack->setCurrentWidget(record);
    m_record = record;
    m_recordFolio = folio;
}

void MainWindow::closeVehicleRecord()
{
    if (!m_record)
        return;
    dropPage(m_record);
    m_record = nullptr;
    m_recordFolio = -1;
}

void MainWindow::onVehicleChanged(int folio)
{
    if (m_inventory)
        m_inventory->vehicleUpdated(folio);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Si el expediente tiene cambios y el usuario se queda con ellos, se le
    // muestra para que siga donde iba.
    if (m_record && !m_record->confirmLeave()) {
        ui->mainContentStack->setCurrentWidget(m_record);
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::setRecordFactory(RecordFactory factory)
{
    m_recordFactory = std::move(factory);
}

void MainWindow::dropPage(QWidget *page)
{
    // El inventario se muestra antes de quitar la página: si se quitara siendo
    // la visible, el stack pasaría por su cuenta a otra página antes de llegar
    // al inventario. Si la visible es otra, se queda donde está.
    if (ui->mainContentStack->currentWidget() == page)
        ui->mainContentStack->setCurrentWidget(ui->inventoryPage);

    // removeWidget() no le quita el padre: la página sigue siendo hija del
    // stack, solo deja de ser una de sus páginas y queda oculta. Así nunca
    // cuenta como ventana de nivel superior, ni por un instante (lo dice la
    // documentación de QStackedWidget y se comprobó con Qt 6.11).
    ui->mainContentStack->removeWidget(page);
    // deleteLater() y no delete: esto suele correr dentro de una señal de la
    // propia página, que sigue a media ejecución. Se destruye cuando el
    // control vuelve al ciclo de eventos.
    page->deleteLater();
}
