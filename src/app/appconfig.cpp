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

} // namespace

void AppConfig::setStorageRoot(const QString &path)
{
    storageRootStorage() = path;
}

QString AppConfig::storageRoot()
{
    return storageRootStorage();
}
