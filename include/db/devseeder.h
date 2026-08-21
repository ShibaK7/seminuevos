#ifndef DEVSEEDER_H
#define DEVSEEDER_H

#include <QString>

class QSqlDatabase;

// Inserta usuarios de prueba para poder ejercitar el login en desarrollo.
// A diferencia de SchemaInitializer (que SIEMPRE debe correr, porque
// garantiza que la estructura exista), esto es una comodidad de desarrollo
// y se controla aparte con SEED_TEST_USERS en el .env -- en un ambiente
// real no se querría crear usuarios de prueba solos en cada arranque.
namespace DevSeeder
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result run(QSqlDatabase &db);
} // namespace DevSeeder

#endif // DEVSEEDER_H
