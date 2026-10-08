#include "presentation/views/shell/mainwindow.h"
#include "ui_mainwindow.h"

#include "presentation/presenters/inventorypresenter.h"
#include "presentation/views/inventory/inventoryview.h"
#include "presentation/views/wizard/vehiclewizardview.h"

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

    // Inventario en el menú solo cambia de página. Un asistente abierto no se
    // cierra por eso: queda vivo y oculto, con lo capturado, y "Agregar
    // Vehículo" lo retoma (ver openVehicleWizard).
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

InventoryView &MainWindow::inventoryView()
{
    return *ui->inventoryPage;
}

void MainWindow::bindInventory(presentation::InventoryPresenter &presenter)
{
    m_inventory = &presenter;
    ui->inventoryPage->bind(presenter);
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
