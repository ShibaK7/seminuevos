#include "app/compositionroot.h"

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/auth/devseeder.h"
#include "adapters/persistence/auth/sqluserdirectory.h"
#include "adapters/persistence/commercial/sqlcommercial.h"
#include "adapters/persistence/commercial/sqlsalerepository.h"
#include "adapters/persistence/common/sqlcounterpartydirectory.h"
#include "adapters/persistence/inventory/sqlinventoryreader.h"
#include "adapters/persistence/inventory/sqlmaintenancerepository.h"
#include "adapters/persistence/inventory/sqlreferencedatareader.h"
#include "adapters/persistence/inventory/sqlvehiclerecordreader.h"
#include "adapters/persistence/inventory/sqlvehiclerepository.h"
#include "adapters/storage/localfilestorage.h"
#include "application/auth/services/authenticationservice.h"
#include "application/commercial/sales/services/saleservice.h"
#include "application/commercial/services/commercialservice.h"
#include "application/common/services/counterpartylookupservice.h"
#include "application/inventory/maintenance/services/maintenanceservice.h"
#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "application/inventory/services/inventoryservice.h"
#include "presentation/commercial/commercialmainview.h"
#include "presentation/commercial/commercialpresenter.h"
#include "presentation/commercial/registration/saledetailspresenter.h"
#include "presentation/commercial/registration/saledetailsview.h"
#include "presentation/commercial/registration/salewizardview.h"
#include "presentation/commercial/saledetails/SaleDetails.h"
#include "presentation/commercial/saledetails/saledetailspresenter.h"
#include "presentation/inventory/inventorypresenter.h"
#include "presentation/inventory/inventoryview.h"
#include "presentation/inventory/maintenance/maintenancehistorypresenter.h"
#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"
#include "presentation/inventory/record/vehiclerecordpresenter.h"
#include "presentation/inventory/record/vehiclerecordview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"
#include "presentation/inventory/registration/vehiclewizardpresenter.h"
#include "presentation/inventory/registration/vehiclewizardview.h"
#include "presentation/login/loginpresenter.h"
#include "presentation/login/loginwindow.h"
#include "presentation/shell/mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace {

// Como la aplicación no termina sola al ocultarse la última ventana (ver
// main.cpp), hay que atender a quien cierra el login sin llegar a entrar: sin
// esto el proceso quedaría vivo y sin ventanas.
class LoginCloseWatcher : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Close)
            QCoreApplication::quit();
        return QObject::eventFilter(watched, event);
    }
};

// La hoja global se aplica al mostrar la ventana principal y no antes: su
// regla genérica QWidget { background-color: #FFFFFF } pisaría el fondo del
// login, que trae su propio estilo.
void applyGlobalStyle()
{
    QFont globalFont = QApplication::font();
    globalFont.setStyleHint(QFont::SansSerif);
    globalFont.setFamilies({"Open Sans", "Segoe UI", "Roboto", "Arial"});
    globalFont.setPointSize(15);
    QApplication::setFont(globalFont);

    QFile styleFile(QStringLiteral(":/styles/global-style-clean.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&styleFile);
        qApp->setStyleSheet(stream.readAll());
    } else {
        qWarning() << "No se pudo abrir la hoja de estilos:" << styleFile.fileName();
    }
}

// Desvanece un widget. Al terminar le quita el efecto: mientras está puesto,
// Qt dibuja la ventana entera de forma indirecta en cada repintado.
QPropertyAnimation *fade(QWidget *widget, qreal from, qreal to)
{
    auto *effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    auto *animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(500);
    animation->setStartValue(from);
    animation->setEndValue(to);
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget] { widget->setGraphicsEffect(nullptr); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    return animation;
}

} // namespace

CompositionRoot::CompositionRoot()
    : m_settings(loadAppSettings())
    , m_pool(m_settings.database)
{
}

CompositionRoot::~CompositionRoot() = default;

bool CompositionRoot::start()
{
    ConnectionPool &pool = m_pool;
    // Todavía no hay ventana: comprobar la base aquí no congela nada. En
    // cuanto termina el arranque, el SQL en el hilo de la interfaz vuelve a
    // estar prohibido (ConnectionPool::acquire lo revisa en Debug).
    pool.setGuiThreadAllowed(true);

    // Antes de mostrar el login se comprueba que la base responda, para avisar
    // con claridad en vez de abrir una app rota. El esquema y los catálogos los
    // crean los scripts de init-db/ cuando Docker arranca con el volumen vacío
    // (docker compose down -v && docker compose up -d los regenera).
    {
        ConnectionPool::Handle handle = pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            QMessageBox::critical(nullptr, QStringLiteral("Error de conexión"),
                QStringLiteral("No se pudo conectar con la base de datos. %1"
                               "Verifica que el contenedor de PostgreSQL esté corriendo "
                               "(docker compose up -d) y que el archivo .env tenga los "
                               "datos correctos.").arg(db.lastError().text()));
            return false;
        }

        if (m_settings.seedTestUsers) {
            const DevSeeder::Result seed = DevSeeder::run(db, m_hasher);
            if (!seed.ok) {
                QMessageBox::critical(nullptr, QStringLiteral("Error al crear usuarios de prueba"),
                                      seed.errorMessage);
                return false;
            }
        }
    }
    pool.setGuiThreadAllowed(false);

    m_users = std::make_unique<SqlUserDirectory>(pool);
    m_auth = std::make_unique<application::AuthenticationService>(*m_users, m_hasher);

    m_vehicles = std::make_unique<SqlVehicleRepository>(pool);
    m_files = std::make_unique<LocalFileStorage>(m_settings.storageRoot);
    m_referenceData = std::make_unique<SqlReferenceDataReader>(pool);
    m_registration = std::make_unique<application::VehicleRegistrationService>(
        *m_vehicles, *m_files, *m_referenceData, m_contracts);
    m_inventoryReader = std::make_unique<SqlInventoryReader>(pool);
    m_inventory = std::make_unique<application::InventoryService>(*m_inventoryReader, *m_files);
    m_counterparties = std::make_unique<SqlCounterpartyDirectory>(pool);
    m_saleRepository = std::make_unique<SqlSaleRepository>(pool);
    m_counterpartyLookup =
        std::make_unique<application::CounterpartyLookupService>(*m_counterparties);
    m_saleService = std::make_unique<application::SaleService>(*m_saleRepository, *m_counterparties);
    m_recordReader = std::make_unique<SqlVehicleRecordReader>(pool);
    m_records = std::make_unique<application::VehicleRecordService>(
        *m_recordReader, *m_vehicles, *m_files, *m_referenceData);
    m_maintenanceRepository = std::make_unique<SqlMaintenanceRepository>(pool);
    m_maintenance = std::make_unique<application::MaintenanceService>(
        *m_maintenanceRepository, *m_referenceData, m_receipts);
    m_commercialReader = std::make_unique<SqlCommercial>(pool);
    m_commercial = std::make_unique<application::CommercialService>(*m_commercialReader, *m_commercialReader);

    m_login = std::make_unique<LoginWindow>();
    m_login->installEventFilter(new LoginCloseWatcher(m_login.get()));
    m_loginPresenter = new presentation::LoginPresenter(*m_login, *m_auth, m_runner, this);

    connect(m_login.get(), &LoginWindow::loginRequested, m_loginPresenter,
            &presentation::LoginPresenter::login);
    connect(m_loginPresenter, &presentation::LoginPresenter::authenticated, this,
            &CompositionRoot::showMain);

    m_login->show();
    return true;
}

void CompositionRoot::showMain(const application::SessionDto &session)
{
    QPropertyAnimation *fadeOut = fade(m_login.get(), 1.0, 0.0);
    connect(fadeOut, &QPropertyAnimation::finished, this, [this, session] {
        m_login->hide();
        applyGlobalStyle();

        auto *mainWindow = new MainWindow();
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(mainWindow, &QObject::destroyed, qApp, &QCoreApplication::quit);
        mainWindow->setSession(session);

        auto &saleDetails = mainWindow->saleWizardView().detailsView();
        auto &saleWizard = mainWindow->saleWizardView();
        auto *salePresenter = new presentation::SaleDetailsPresenter(
            saleDetails, *m_counterpartyLookup, m_runner, mainWindow, m_saleService.get());
        connect(&saleDetails, &SaleDetailsView::counterpartySearchRequested, salePresenter,
                &presentation::SaleDetailsPresenter::searchCounterparty);
        connect(&saleDetails, &SaleDetailsView::vehicleSearchRequested, salePresenter,
                &presentation::SaleDetailsPresenter::searchVehicle);
        connect(&saleDetails, &SaleDetailsView::vendorSearchRequested, salePresenter,
                &presentation::SaleDetailsPresenter::searchVendors);
        saleDetails.setCurrentUser(session);
        connect(&saleWizard, &SaleWizardView::saveSaleRequested, salePresenter,
                [salePresenter, &saleDetails] { salePresenter->registerSale(saleDetails.registrationForm()); });
        connect(salePresenter, &presentation::SaleDetailsPresenter::saleRegistrationStarted,
                &saleWizard, [&saleWizard] { saleWizard.setSaving(true); });
        connect(salePresenter, &presentation::SaleDetailsPresenter::saleRegistrationFinished,
                &saleWizard, [&saleWizard](bool success, int folio, const QString &message) {
                    saleWizard.setSaving(false);
                    saleWizard.showSaveResult(success, folio, message);
                });

        auto *inventory = new presentation::InventoryPresenter(
            mainWindow->inventoryView(), *m_inventory, m_runner, mainWindow);
        mainWindow->bindInventory(*inventory);
        inventory->reload();

        auto *commercial = new presentation::CommercialPresenter(
            mainWindow->commercialView(), *m_commercial, m_runner, mainWindow);
        mainWindow->bindCommercial(*commercial);
        commercial->reload();

        mainWindow->setSaleDetailsFactory([this, role = session.role](QWidget *parent, int folio) {
            auto *view = new SaleDetails(role, parent);
            auto *presenter = new presentation::CommercialSaleDetailsPresenter(
                *view, *m_commercial, m_runner, folio, role, view);
            view->bind(*presenter);
            presenter->load();
            return view;
        });

        mainWindow->setWizardFactory([this](QWidget *parent) {
            auto *view = new VehicleWizardView(parent);
            auto *presenter = new presentation::VehicleWizardPresenter(
                *view, view->detailsView(), view->conditionsView(), view->filesView(),
                *m_registration, m_runner, m_settings.wizardFreeNavigation, view);
            view->bind(*presenter);
            presenter->start();
            return view;
        });

        mainWindow->setRecordFactory([this, role = session.role](QWidget *parent, int folio) {
            auto *view = new VehicleRecordView(parent);
            auto *presenter = new presentation::VehicleRecordPresenter(
                *view, view->detailsView(), view->conditionsView(), view->filesView(),
                *m_registration, *m_records, m_runner, folio, role, view);
            view->bind(*presenter);
            MaintenanceHistoryTableWidget &history = view->detailsView().maintenanceHistory();
            auto *maintenance = new presentation::MaintenanceHistoryPresenter(
                history, *m_maintenance, m_runner, folio, role, presenter);
            history.bind(*maintenance);
            connect(maintenance, &presentation::MaintenanceHistoryPresenter::maintenanceCostChanged,
                    presenter, &presentation::VehicleRecordPresenter::showMaintenanceCost);
            presenter->start();
            maintenance->start();
            return view;
        });
        m_main = mainWindow;

        mainWindow->showMaximized();
        fade(mainWindow, 0.0, 1.0);
    });
}
