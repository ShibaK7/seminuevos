#include "include/app/appconfig.h"
#include "include/loginwindow.h"
#include "include/mainwindow.h"
#include "include/db/connectionpool.h"
#include "include/db/catalogseeder.h"
#include "include/db/conditioncatalogseeder.h"
#include "include/db/devseeder.h"
#include "include/db/schemainitializer.h"
#include "include/auth/loginworker.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QMap>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>

namespace {

// Lee un archivo .env simple (KEY=VALUE por línea, "#" para comentarios).
// No falla si no existe: el llamador aplica sus propios valores por defecto.
QMap<QString, QString> readEnvFile(const QString &path)
{
    QMap<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        values.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return values;
}

// Una app de escritorio (WIN32_EXECUTABLE) no hereda variables de entorno de
// shell de forma confiable al abrirse desde el IDE o doble clic, así que la
// config de conexión se lee de un .env en vez de depender de eso. Se busca
// primero junto al ejecutable (caso de una build empaquetada) y si no,
// junto al código fuente (caso de desarrollo, vía la macro de CMake
// PROJECT_SOURCE_DIR -- así funciona sin importar dónde termine el
// directorio de build).
QMap<QString, QString> loadDatabaseEnv()
{
    QMap<QString, QString> env = readEnvFile(QCoreApplication::applicationDirPath() + "/.env");
    if (env.isEmpty())
        env = readEnvFile(QStringLiteral(PROJECT_SOURCE_DIR) + "/.env");
    return env;
}

// Como la aplicación ya no termina sola al ocultarse la última ventana, hay
// que atender el caso de quien cierra el login sin llegar a entrar: sin esto
// el proceso se quedaría vivo y sin ventanas. No necesita Q_OBJECT porque solo
// redefine un método virtual, sin señales ni slots propios.
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

} // namespace

void applyGlobalStyle(QApplication &app) {
    QFile styleFile(":/resourcess/styles/styles/global-style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        printf("Loading style sheet...");
        QTextStream stream(&styleFile);
        app.setStyleSheet(stream.readAll());
        styleFile.close();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Seminuevos");

    // La aplicación termina SOLO cuando se destruye la ventana principal (ver
    // el connect más abajo). Sin esto habría un segundo camino de salida: Qt
    // cierra la aplicación por su cuenta cuando cree que se ocultó la última
    // ventana, y eso se dispara en momentos que no son un cierre de verdad --
    // entre ocultar el login y mostrar la ventana principal, o cuando un
    // widget deja de tener padre por un instante al reemplazar una página.
    app.setQuitOnLastWindowClosed(false);

    const QMap<QString, QString> env = loadDatabaseEnv();

    DatabaseConfig dbConfig;
    dbConfig.hostName = env.value(QStringLiteral("POSTGRES_HOST"), QStringLiteral("localhost"));
    dbConfig.port = env.value(QStringLiteral("POSTGRES_PORT"), QStringLiteral("5432")).toInt();
    dbConfig.databaseName = env.value(QStringLiteral("POSTGRES_DB"), QStringLiteral("seminuevos"));
    dbConfig.userName = env.value(QStringLiteral("POSTGRES_USER"), QStringLiteral("seminuevos_app"));
    dbConfig.password = env.value(QStringLiteral("POSTGRES_PASSWORD"));
    ConnectionPool::configure(dbConfig);

    // Se resuelve una vez aquí, no en cada pantalla que necesite archivos: lo
    // usan tanto el asistente al copiar fotos como la rejilla de inventario al
    // mostrarlas, porque en la base las rutas se guardan relativas a esta raíz.
    {
        QString storageRoot = env.value(QStringLiteral("STORAGE_ROOT")).trimmed();
        if (storageRoot.isEmpty())
            storageRoot = QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/storage");
        AppConfig::setStorageRoot(storageRoot);
    }

    // Antes de mostrar el login dejamos el esquema al día. El script solo se
    // ejecuta cuando la versión instalada no coincide con
    // SchemaInitializer::kSchemaVersion; en un arranque normal esto no toca
    // nada. Si la base no está disponible, avisamos con claridad en vez de
    // abrir una app rota.
    {
        ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            QMessageBox::critical(nullptr, QStringLiteral("Error de conexión"),
                QStringLiteral("No se pudo conectar con la base de datos.\n\n%1\n\n"
                                "Verifica que el contenedor de PostgreSQL esté corriendo "
                                "(docker compose up -d) y que el archivo .env tenga los "
                                "datos correctos.").arg(db.lastError().text()));
            return 1;
        }

        // Aplicar el esquema BORRA la base. Por defecto se permite (estamos
        // en fase de definición y regenerar es más barato que migrar), pero
        // el .env puede desactivarlo para que un ambiente con datos reales
        // no se vacíe solo al instalar una build nueva.
        const bool allowSchemaReset =
            env.value(QStringLiteral("SCHEMA_AUTO_RESET"), QStringLiteral("true"))
                .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0;

        const SchemaInitializer::Result schemaResult = SchemaInitializer::run(db, allowSchemaReset);
        if (!schemaResult.ok) {
            QMessageBox::critical(nullptr, QStringLiteral("Error al inicializar la base de datos"),
                schemaResult.errorMessage);
            return 1;
        }

        // Catálogo REAL del checklist de condición (US-03.2). Va fuera de la
        // bandera SEED_TEST_USERS a propósito: el Paso 2 del wizard construye
        // sus renglones leyendo vehicle_conditions_cat, así que sin esto la
        // pantalla saldría vacía en cualquier ambiente. Es idempotente.
        const ConditionCatalogSeeder::Result conditionCatalogResult = ConditionCatalogSeeder::run(db);
        if (!conditionCatalogResult.ok) {
            QMessageBox::critical(nullptr, QStringLiteral("Error al sembrar el catálogo de condiciones"),
                conditionCatalogResult.errorMessage);
            return 1;
        }

        // A diferencia del esquema, los usuarios de prueba son solo una
        // comodidad de desarrollo -- se insertan nada más si el .env lo pide
        // explícitamente, para no crear cuentas de prueba en cualquier
        // ambiente sin que nadie lo haya decidido.
        if (env.value(QStringLiteral("SEED_TEST_USERS")).compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
            const DevSeeder::Result seedResult = DevSeeder::run(db);
            if (!seedResult.ok) {
                QMessageBox::critical(nullptr, QStringLiteral("Error al crear usuarios de prueba"),
                    seedResult.errorMessage);
                return 1;
            }

            // Catálogos dummy (tipos/subtipos de vehículo, marcas, combustibles)
            // para que los combos del wizard de registro no queden vacíos --
            // ver comentario en catalogseeder.h.
            const CatalogSeeder::Result catalogResult = CatalogSeeder::run(db);
            if (!catalogResult.ok) {
                QMessageBox::critical(nullptr, QStringLiteral("Error al crear catálogos de prueba"),
                    catalogResult.errorMessage);
                return 1;
            }
        }

        // El aviso del reset va AL FINAL, no justo después de aplicarlo: es un
        // diálogo modal, y entre el reset y este punto se siembran el catálogo
        // de condiciones y los datos de desarrollo. Avisar antes dejaba la
        // aplicación esperando un clic con la base a medio poblar, y cerrarla
        // ahí la dejaba sin catálogo -- con el Paso 2 del wizard en blanco.
        if (schemaResult.applied && schemaResult.hadExistingSchema) {
            QMessageBox::information(nullptr, QStringLiteral("Base de datos regenerada"),
                QStringLiteral("La base se recreó desde cero y se perdieron los datos que "
                                "tenía (esquema %1 -> %2).\n\n"
                                "Es el comportamiento esperado mientras el esquema sigue en "
                                "definición. Para desactivarlo, pon SCHEMA_AUTO_RESET=false "
                                "en el archivo .env.")
                    .arg(schemaResult.previousVersion < 0
                             ? QStringLiteral("sin versionar")
                             : QString::number(schemaResult.previousVersion))
                    .arg(SchemaInitializer::kSchemaVersion));
        }
    }

    LoginWindow login;
    LoginCloseWatcher loginCloseWatcher(&login);
    login.installEventFilter(&loginCloseWatcher);

    // Conectar login exitoso con animación
    QObject::connect(&login, &LoginWindow::loginRequested, [&login, &app](const QString &user, const QString &pass) {
        auto *worker = new LoginWorker(user, pass);

        QObject::connect(worker, &LoginWorker::succeeded, &login,
            [&login, &app](const QString &displayName, const QString &role) {
                Q_UNUSED(displayName);
                Q_UNUSED(role);

                // Animación de desvanecimiento para login
                QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect();
                login.setGraphicsEffect(effect);

                QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
                anim->setDuration(500);
                anim->setStartValue(1.0);
                anim->setEndValue(0.0);
                anim->start();

                login.setBusy(false);
                // Abrir la ventana principal
                QObject::connect(anim, &QPropertyAnimation::finished, [&]() {
                    login.hide();
                    // Crear MainWindow solo después del login exitoso
                    applyGlobalStyle(app);
                    MainWindow *mainWin = new MainWindow();
                    mainWin->showMaximized(); // ← Aquí se maximiza automáticamente

                    //  Cerrar la aplicación cuando se cierre MainWindow
                    QObject::connect(mainWin, &MainWindow::destroyed, &app, &QApplication::quit);

                    // Animación de aparición para mainWin
                    QGraphicsOpacityEffect *effect2 = new QGraphicsOpacityEffect();
                    mainWin->setGraphicsEffect(effect2);

                    QPropertyAnimation *anim2 = new QPropertyAnimation(effect2, "opacity");
                    anim2->setDuration(500);
                    anim2->setStartValue(0.0);
                    anim2->setEndValue(1.0);
                    // Se retira el efecto al terminar la animación. Mientras
                    // sigue puesto, Qt dibuja la ventana entera de forma
                    // indirecta (la pinta a una imagen y luego la compone), y
                    // eso se paga en cada repintado y da problemas con los
                    // widgets que se agregan o quitan después. La animación
                    // dura medio segundo; el efecto no tiene por qué durar
                    // toda la sesión.
                    QObject::connect(anim2, &QPropertyAnimation::finished, mainWin,
                                     [mainWin] { mainWin->setGraphicsEffect(nullptr); });
                    anim2->start();
                });
            });

        QObject::connect(worker, &LoginWorker::failed, &login, [&login](const QString &reason) {
            login.setBusy(false);
            login.showLoginError(reason);
        });

        QObject::connect(worker, &LoginWorker::finished, worker, &QObject::deleteLater);

        worker->start();
    });

    login.show();
    return app.exec();
}
