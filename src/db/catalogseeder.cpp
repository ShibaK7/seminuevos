#include "../../include/db/catalogseeder.h"

#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

struct VehicleTypeSeed
{
    QString name;
    QStringList subtypes;
};

// Placeholder mientras el negocio entrega el catálogo real de tipos/
// subtipos, marcas y combustibles -- ver comentario en catalogseeder.h.
const QList<VehicleTypeSeed> &vehicleTypeSeeds()
{
    static const QList<VehicleTypeSeed> seeds = {
        {QStringLiteral("Auto"), {QStringLiteral("Sedán"), QStringLiteral("Hatchback"), QStringLiteral("Compacto")}},
        {QStringLiteral("Camioneta"), {QStringLiteral("Pickup"), QStringLiteral("Van")}},
        {QStringLiteral("SUV"), {QStringLiteral("SUV Compacta"), QStringLiteral("SUV Grande")}},
        {QStringLiteral("Motocicleta"), {QStringLiteral("Deportiva"), QStringLiteral("Scooter")}},
    };
    return seeds;
}

const QStringList &brandSeeds()
{
    static const QStringList brands = {
        QStringLiteral("Nissan"), QStringLiteral("Toyota"), QStringLiteral("Chevrolet"),
        QStringLiteral("Ford"), QStringLiteral("Volkswagen"), QStringLiteral("Honda"),
        QStringLiteral("Kia"), QStringLiteral("Hyundai"), QStringLiteral("Mazda"), QStringLiteral("Renault"),
    };
    return brands;
}

const QStringList &fuelTypeSeeds()
{
    static const QStringList fuels = {
        QStringLiteral("Gasolina"), QStringLiteral("Diésel"), QStringLiteral("Híbrido"),
        QStringLiteral("Eléctrico"), QStringLiteral("Gas LP"),
    };
    return fuels;
}

// Inserta (si falta) una categoría de vehículo y devuelve su id, ya sea
// recién creada o ya existente. parentId inválido (QVariant nulo) = tipo de
// nivel superior.
int upsertCategory(QSqlDatabase &db, const QString &name, const QVariant &parentId, QString &errorMessage)
{
    QSqlQuery insertQuery(db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO vehicle_categories_cat (name, parent_id) VALUES (:name, :parent_id) "
        "ON CONFLICT ON CONSTRAINT unique_name_per_parent DO NOTHING"));
    insertQuery.bindValue(QStringLiteral(":name"), name);
    insertQuery.bindValue(QStringLiteral(":parent_id"), parentId);
    if (!insertQuery.exec()) {
        errorMessage = insertQuery.lastError().text();
        return -1;
    }

    QSqlQuery selectQuery(db);
    if (parentId.isValid() && !parentId.isNull()) {
        selectQuery.prepare(QStringLiteral(
            "SELECT id FROM vehicle_categories_cat WHERE name = :name AND parent_id = :parent_id"));
        selectQuery.bindValue(QStringLiteral(":parent_id"), parentId);
    } else {
        selectQuery.prepare(QStringLiteral(
            "SELECT id FROM vehicle_categories_cat WHERE name = :name AND parent_id IS NULL"));
    }
    selectQuery.bindValue(QStringLiteral(":name"), name);

    if (!selectQuery.exec() || !selectQuery.next()) {
        errorMessage = selectQuery.lastError().text();
        return -1;
    }
    return selectQuery.value(0).toInt();
}

bool seedVehicleCategories(QSqlDatabase &db, QString &errorMessage)
{
    for (const VehicleTypeSeed &type : vehicleTypeSeeds()) {
        const int typeId = upsertCategory(db, type.name, QVariant(QMetaType(QMetaType::Int)), errorMessage);
        if (typeId < 0)
            return false;

        for (const QString &subtypeName : type.subtypes) {
            if (upsertCategory(db, subtypeName, typeId, errorMessage) < 0)
                return false;
        }
    }
    return true;
}

bool seedSimpleCatalog(QSqlDatabase &db, const QString &table, const QStringList &names, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO %1 (name) VALUES (:name) ON CONFLICT (name) DO NOTHING").arg(table));

    for (const QString &name : names) {
        query.bindValue(QStringLiteral(":name"), name);
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

CatalogSeeder::Result CatalogSeeder::run(QSqlDatabase &db)
{
    Result result;

    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1").arg(db.lastError().text());
        return result;
    }

    QString errorMessage;
    if (!seedVehicleCategories(db, errorMessage)
        || !seedSimpleCatalog(db, QStringLiteral("brands_cat"), brandSeeds(), errorMessage)
        || !seedSimpleCatalog(db, QStringLiteral("fuel_type_cat"), fuelTypeSeeds(), errorMessage)) {
        result.errorMessage = QStringLiteral("Error sembrando catálogos dummy: %1").arg(errorMessage);
        db.rollback();
        return result;
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1").arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    return result;
}
