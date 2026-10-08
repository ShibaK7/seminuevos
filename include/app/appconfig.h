#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

// Ajustes de la aplicación que se resuelven una sola vez al arrancar y que
// varias pantallas necesitan después.
//
// Existe porque el parser del .env estaba a punto de escribirse por tercera
// vez: main.cpp lo tiene para la conexión, el asistente de registro lo repitió
// para averiguar dónde guardar los archivos, y ahora la rejilla de inventario
// necesita ese mismo dato para poder resolver las rutas de las fotos, que en la
// base se guardan relativas. Se sigue el mismo patrón que ConnectionPool:
// configure() al arrancar, consultas de solo lectura después.
namespace AppConfig
{
// Raíz del almacén de archivos: {storageRoot}/vehicles/{vin}/...
void setStorageRoot(const QString &path);
QString storageRoot();

// Navegación libre en el asistente de registro: deja ir a cualquier paso sin
// validar los anteriores (WIZARD_FREE_NAVIGATION en el .env). Es solo para
// desarrollo, para trabajar estilos sin comentar la validación. Apagada por
// omisión.
void setWizardFreeNavigation(bool enabled);
bool wizardFreeNavigation();
} // namespace AppConfig

#endif // APPCONFIG_H
