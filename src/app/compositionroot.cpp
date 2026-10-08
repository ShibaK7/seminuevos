#include "app/compositionroot.h"

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/devseeder.h"
#include "adapters/persistence/sqlreferencedatareader.h"
#include "adapters/persistence/sqluserdirectory.h"
#include "adapters/persistence/sqlvehiclerepository.h"
#include "adapters/storage/localfilestorage.h"
#include "app/appconfig.h"
#include "application/services/authenticationservice.h"
#include "application/services/vehicleregistrationservice.h"
#include "presentation/presenters/loginpresenter.h"
#include "presentation/presenters/vehiclewizardpresenter.h"
#include "presentation/views/login/loginwindow.h"
#include "presentation/views/shell/mainwindow.h"
#include "presentation/views/wizard/vehicleconditionsview.h"
#include "presentation/views/wizard/vehicledetailsview.h"
#include "presentation/views/wizard/vehiclefilesview.h"
#include "presentation/views/wizard/vehiclewizardview.h"

#include <QApplication>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QSqlDatabase>
#include <QSqlError>
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

    QFile styleFile(QStringLiteral(":/resourcess/styles/styles/global-style-clean.qss"));
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
{
}

CompositionRoot::~CompositionRoot() = default;

bool CompositionRoot::start()
{
    ConnectionPool::configure(m_settings.database);
    ConnectionPool &pool = ConnectionPool::instance();

    // Mientras las vistas del inventario y del asistente no reciban sus
    // servicios por inyección, siguen leyendo esto de AppConfig.
    AppConfig::setStorageRoot(m_settings.storageRoot);
    AppConfig::setWizardFreeNavigation(m_settings.wizardFreeNavigation);

    // Antes de mostrar el login se comprueba que la base responda, para avisar
    // con claridad en vez de abrir una app rota. El esquema y los catálogos los
    // crean los scripts de init-db/ cuando Docker arranca con el volumen vacío
    // (docker compose down -v && docker compose up -d los regenera).
    {
        ConnectionPool::Handle handle = pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            QMessageBox::critical(nullptr, QStringLiteral("Error de conexión"),
                QStringLiteral("No se pudo conectar con la base de datos.\n\n%1\n\n"
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

    m_users = std::make_unique<SqlUserDirectory>(pool);
    m_auth = std::make_unique<application::AuthenticationService>(*m_users, m_hasher);

    m_vehicles = std::make_unique<SqlVehicleRepository>(pool);
    m_files = std::make_unique<LocalFileStorage>(m_settings.storageRoot);
    m_referenceData = std::make_unique<SqlReferenceDataReader>(pool);
    m_registration = std::make_unique<application::VehicleRegistrationService>(
        *m_vehicles, *m_files, *m_referenceData, m_contracts);

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
    Q_UNUSED(session); // el rol se mostrará cuando MainWindow reciba la sesión

    QPropertyAnimation *fadeOut = fade(m_login.get(), 1.0, 0.0);
    connect(fadeOut, &QPropertyAnimation::finished, this, [this] {
        m_login->hide();
        applyGlobalStyle();

        auto *mainWindow = new MainWindow();
        // Cerrar la ventana principal la destruye, y eso termina la app (ver
        // main.cpp). Sin WA_DeleteOnClose solo se ocultaba y el proceso seguía
        // vivo, bloqueando el .exe para la siguiente compilación.
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(mainWindow, &QObject::destroyed, qApp, &QCoreApplication::quit);
        // El asistente se arma aquí porque solo la raíz tiene su servicio y el
        // TaskRunner; MainWindow solo decide cuándo abrirlo.
        mainWindow->setWizardFactory([this](QWidget *parent) {
            auto *view = new VehicleWizardView(parent);
            // El presenter vive lo que vive la vista (es su hijo): si el
            // asistente se cierra con un registro en curso, el resultado ya no
            // se entrega.
            auto *presenter = new presentation::VehicleWizardPresenter(
                *view, view->detailsView(), view->conditionsView(), view->filesView(),
                *m_registration, m_runner, m_settings.wizardFreeNavigation, view);
            view->bind(*presenter);
            presenter->start();
            return view;
        });
        m_main = mainWindow;

        mainWindow->showMaximized();
        fade(mainWindow, 0.0, 1.0);
    });
}
