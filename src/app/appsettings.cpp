#include "app/appsettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QMap>
#include <QTextStream>

namespace {

// Lee un .env simple (KEY=VALUE por línea, "#" para comentarios). No falla si
// no existe: quien llama aplica sus valores por defecto.
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

// Solo un "true" o un "1" explícitos encienden una bandera: si falta o trae
// otra cosa, queda apagada, que es lo seguro.
bool isEnabled(const QString &value)
{
    const QString v = value.trimmed();
    return v.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || v == QStringLiteral("1");
}

} // namespace

AppSettings loadAppSettings()
{
    QMap<QString, QString> env = readEnvFile(QCoreApplication::applicationDirPath() + "/.env");
    if (env.isEmpty())
        env = readEnvFile(QStringLiteral(PROJECT_SOURCE_DIR) + "/.env");

    AppSettings settings;
    settings.database.hostName = env.value(QStringLiteral("POSTGRES_HOST"), QStringLiteral("localhost"));
    settings.database.port = env.value(QStringLiteral("POSTGRES_PORT"), QStringLiteral("5432")).toInt();
    settings.database.databaseName = env.value(QStringLiteral("POSTGRES_DB"), QStringLiteral("seminuevos"));
    settings.database.userName = env.value(QStringLiteral("POSTGRES_USER"), QStringLiteral("seminuevos_app"));
    settings.database.password = env.value(QStringLiteral("POSTGRES_PASSWORD"));

    settings.storageRoot = env.value(QStringLiteral("STORAGE_ROOT")).trimmed();
    if (settings.storageRoot.isEmpty())
        settings.storageRoot = QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/storage");

    settings.seedTestUsers = isEnabled(env.value(QStringLiteral("SEED_TEST_USERS")));
    settings.wizardFreeNavigation = isEnabled(env.value(QStringLiteral("WIZARD_FREE_NAVIGATION")));
    return settings;
}
