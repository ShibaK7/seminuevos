#ifndef APP_COMPOSITIONROOT_H
#define APP_COMPOSITIONROOT_H

#include "adapters/contract/pdfcontractgenerator.h"
#include "adapters/persistence/connectionpool.h"
#include "adapters/security/pbkdf2passwordhasher.h"
#include "app/appsettings.h"
#include "application/auth/dto/authdtos.h"
#include "presentation/tasks/pooledtaskrunner.h"

#include <QObject>
#include <QPointer>

#include <memory>

class LoginWindow;
class MainWindow;
class LocalFileStorage;
class SqlInventoryReader;
class SqlReferenceDataReader;
class SqlUserDirectory;
class SqlVehicleRepository;

namespace application {
class AuthenticationService;
class InventoryService;
class VehicleRegistrationService;
}
namespace presentation {
class LoginPresenter;
}

// Raíz de composición: el único lugar que conoce todas las capas. Crea los
// adaptadores, los inyecta en los casos de uso, arma los presenters con sus
// vistas y decide qué ventana se ve. Antes todo esto vivía en lambdas
// anidadas de main.cpp.
//
// El orden de los miembros es el orden de vida: lo que se declara al final se
// destruye primero. El TaskRunner va al final para que, al cerrar, espere las
// tareas en curso antes de que se destruyan los servicios que usan.
class CompositionRoot final : public QObject
{
    Q_OBJECT

public:
    CompositionRoot();
    ~CompositionRoot() override;

    // Comprueba la base, siembra usuarios de prueba si el .env lo pide y
    // muestra el inicio de sesión. Devuelve false si la app no puede arrancar
    // (el usuario ya vio por qué).
    bool start();

private:
    void showMain(const application::SessionDto &session);

    AppSettings m_settings;
    ConnectionPool m_pool;
    Pbkdf2PasswordHasher m_hasher;
    std::unique_ptr<SqlUserDirectory> m_users;
    std::unique_ptr<application::AuthenticationService> m_auth;
    std::unique_ptr<SqlVehicleRepository> m_vehicles;
    std::unique_ptr<LocalFileStorage> m_files;
    std::unique_ptr<SqlReferenceDataReader> m_referenceData;
    PdfContractGenerator m_contracts;
    std::unique_ptr<application::VehicleRegistrationService> m_registration;
    std::unique_ptr<SqlInventoryReader> m_inventoryReader;
    std::unique_ptr<application::InventoryService> m_inventory;
    std::unique_ptr<LoginWindow> m_login;
    presentation::LoginPresenter *m_loginPresenter = nullptr;
    QPointer<MainWindow> m_main;
    presentation::PooledTaskRunner m_runner;
};

#endif // APP_COMPOSITIONROOT_H
