#include "../../include/app/appconfig.h"

namespace {

// Se fija en el arranque, antes de que exista cualquier ventana, y a partir de
// ahí solo se lee. No hace falta sincronización: nadie lo escribe una vez que
// la aplicación está corriendo.
QString &storageRootStorage()
{
    static QString value;
    return value;
}

// Mismo criterio que storageRootStorage(): se fija al arrancar y después solo
// se lee. Nace en false para que, si nadie la configura, el asistente valide.
bool &wizardFreeNavigationStorage()
{
    static bool value = false;
    return value;
}

} // namespace

void AppConfig::setStorageRoot(const QString &path)
{
    storageRootStorage() = path;
}

QString AppConfig::storageRoot()
{
    return storageRootStorage();
}

void AppConfig::setWizardFreeNavigation(bool enabled)
{
    wizardFreeNavigationStorage() = enabled;
}

bool AppConfig::wizardFreeNavigation()
{
    return wizardFreeNavigationStorage();
}
