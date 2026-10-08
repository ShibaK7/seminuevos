#ifndef ADAPTERS_PERSISTENCE_DEVSEEDER_H
#define ADAPTERS_PERSISTENCE_DEVSEEDER_H

#include <QString>

class QSqlDatabase;

namespace application {
class PasswordHasher;
}

// Inserta usuarios de prueba para poder ejercitar el login en desarrollo.
// A diferencia del esquema y los catálogos (que crean los scripts de init-db/
// cuando el contenedor arranca con el volumen vacío, porque sin ellos la app
// no funciona), esto es una comodidad de desarrollo y se controla aparte con
// SEED_TEST_USERS en el .env -- en un ambiente real no se querría crear
// usuarios de prueba solos en cada arranque.
//
// Recibe el hasher por el puerto de la aplicación en vez de usar el adaptador
// de seguridad directamente: así la persistencia no depende de otro adaptador.
namespace DevSeeder
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result run(QSqlDatabase &db, const application::PasswordHasher &hasher);
} // namespace DevSeeder

#endif // ADAPTERS_PERSISTENCE_DEVSEEDER_H
