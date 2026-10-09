#ifndef APP_APPSETTINGS_H
#define APP_APPSETTINGS_H

#include "adapters/persistence/connectionpool.h"

#include <QString>

// Configuración de arranque leída del .env (antes vivía suelta en main.cpp).
//
// Una app de escritorio (WIN32_EXECUTABLE) no hereda variables de entorno de
// shell de forma confiable al abrirse desde el IDE o con doble clic, así que la
// configuración se lee de un .env. Se busca primero junto al ejecutable (build
// empaquetada) y si no, junto al código fuente (desarrollo, vía la macro de
// CMake PROJECT_SOURCE_DIR).
struct AppSettings
{
    DatabaseConfig database;
    // Raíz de fotos y documentos (storage/vehicles/{vin}/...). En la base se
    // guardan rutas relativas a esta carpeta.
    QString storageRoot;
    // Usuarios de prueba (incluido a/a). Solo desarrollo.
    bool seedTestUsers = false;
    // Navegación libre en el asistente, para trabajar estilos. Solo desarrollo.
    bool wizardFreeNavigation = false;
};

AppSettings loadAppSettings();

#endif // APP_APPSETTINGS_H
