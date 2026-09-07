#include "../../include/db/conditioncatalogseeder.h"

#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

struct ConditionSeed
{
    QString itemKey;
    QString category;
    QString element;
    QString negativeLabel;
};

// Los 37 ítems del checklist, EN EL ORDEN en que deben aparecer en el Paso 2:
// el sort_order se deriva del índice, así que reordenar esta lista reordena
// la UI. Los grupos deben quedar contiguos aquí -- la vista abre un
// encabezado nuevo cada vez que cambia la categoría, y una categoría partida
// en dos produciría dos encabezados con el mismo nombre.
//
// La lista vive en el .cpp del seeder, no en un header público, a propósito:
// desde que la BD es la fuente de verdad del checklist, nadie más debe poder
// compilar contra estos datos. Si estuviera en un header, tarde o temprano
// alguien la incluiría desde la UI "para no hacer un query" y el modelo se
// deshace sin que nadie lo note.
//
// negativeLabel es la etiqueta del radio negativo y varía por ítem
// ("Deteriorada" para pintura, "Gastadas" para llantas). Es presentación:
// lo que se guarda en vehicle_inspection es un booleano.
const QList<ConditionSeed> &conditionSeeds()
{
    static const QList<ConditionSeed> seeds = {
        // Exteriores
        {QStringLiteral("luces_delanteras"), QStringLiteral("Exteriores"), QStringLiteral("Luces delanteras"), QStringLiteral("Con fallas")},
        {QStringLiteral("luces_traseras"), QStringLiteral("Exteriores"), QStringLiteral("Luces traseras"), QStringLiteral("Con fallas")},
        {QStringLiteral("espejos"), QStringLiteral("Exteriores"), QStringLiteral("Espejos"), QStringLiteral("Con fallas")},
        {QStringLiteral("espejo_retrovisor"), QStringLiteral("Exteriores"), QStringLiteral("Espejo retrovisor"), QStringLiteral("Con fallas")},
        {QStringLiteral("antena"), QStringLiteral("Exteriores"), QStringLiteral("Antena"), QStringLiteral("Con fallas")},
        {QStringLiteral("molduras"), QStringLiteral("Exteriores"), QStringLiteral("Molduras"), QStringLiteral("Con fallas")},
        {QStringLiteral("parabrisas"), QStringLiteral("Exteriores"), QStringLiteral("Parabrisas"), QStringLiteral("Con fallas")},
        {QStringLiteral("tapon_gasolina"), QStringLiteral("Exteriores"), QStringLiteral("Tapón de gasolina"), QStringLiteral("Con fallas")},
        {QStringLiteral("pintura"), QStringLiteral("Exteriores"), QStringLiteral("Pintura"), QStringLiteral("Deteriorada")},
        {QStringLiteral("hojalateria"), QStringLiteral("Exteriores"), QStringLiteral("Hojalatería"), QStringLiteral("Deteriorada")},

        // Llantas y rines
        {QStringLiteral("llantas"), QStringLiteral("Llantas y rines"), QStringLiteral("Llantas"), QStringLiteral("Gastadas")},
        {QStringLiteral("llanta_refaccion"), QStringLiteral("Llantas y rines"), QStringLiteral("Llanta de refacción"), QStringLiteral("Con fallas")},
        {QStringLiteral("condicion_rines"), QStringLiteral("Llantas y rines"), QStringLiteral("Condición de rines"), QStringLiteral("Con fallas")},
        {QStringLiteral("tapones_llanta"), QStringLiteral("Llantas y rines"), QStringLiteral("Tapones de llanta"), QStringLiteral("Con fallas")},
        {QStringLiteral("gato_maneral"), QStringLiteral("Llantas y rines"), QStringLiteral("Gato y maneral"), QStringLiteral("Con fallas")},
        {QStringLiteral("llave_rueda"), QStringLiteral("Llantas y rines"), QStringLiteral("Llave de rueda"), QStringLiteral("Con fallas")},
        {QStringLiteral("herramientas"), QStringLiteral("Llantas y rines"), QStringLiteral("Herramientas"), QStringLiteral("Con fallas")},
        {QStringLiteral("triangulo_seguridad"), QStringLiteral("Llantas y rines"), QStringLiteral("Triángulo de seguridad"), QStringLiteral("Con fallas")},

        // Interiores
        {QStringLiteral("interiores"), QStringLiteral("Interiores"), QStringLiteral("Interiores"), QStringLiteral("Deteriorados")},
        {QStringLiteral("asientos"), QStringLiteral("Interiores"), QStringLiteral("Asientos"), QStringLiteral("Con fallas")},
        {QStringLiteral("tapetes"), QStringLiteral("Interiores"), QStringLiteral("Tapetes"), QStringLiteral("Con fallas")},
        {QStringLiteral("alfombras"), QStringLiteral("Interiores"), QStringLiteral("Alfombras"), QStringLiteral("Con fallas")},
        {QStringLiteral("manijas"), QStringLiteral("Interiores"), QStringLiteral("Manijas"), QStringLiteral("Con fallas")},
        {QStringLiteral("cinturon_seguridad"), QStringLiteral("Interiores"), QStringLiteral("Cinturón de seguridad"), QStringLiteral("Con fallas")},
        {QStringLiteral("ceniceros"), QStringLiteral("Interiores"), QStringLiteral("Ceniceros"), QStringLiteral("Con fallas")},
        {QStringLiteral("encendedor"), QStringLiteral("Interiores"), QStringLiteral("Encendedor"), QStringLiteral("Con fallas")},
        {QStringLiteral("radio"), QStringLiteral("Interiores"), QStringLiteral("Radio"), QStringLiteral("Con fallas")},
        {QStringLiteral("bocinas"), QStringLiteral("Interiores"), QStringLiteral("Bocinas"), QStringLiteral("Con fallas")},
        {QStringLiteral("instrumentos_tablero"), QStringLiteral("Interiores"), QStringLiteral("Instrumentos de tablero"), QStringLiteral("Con fallas")},
        {QStringLiteral("claxon"), QStringLiteral("Interiores"), QStringLiteral("Claxon"), QStringLiteral("Con fallas")},
        {QStringLiteral("limpiadores"), QStringLiteral("Interiores"), QStringLiteral("Limpiadores"), QStringLiteral("Con fallas")},

        // Mecánica
        {QStringLiteral("motor"), QStringLiteral("Mecánica"), QStringLiteral("Motor"), QStringLiteral("Con fallas")},
        {QStringLiteral("frenos"), QStringLiteral("Mecánica"), QStringLiteral("Frenos"), QStringLiteral("Con fallas")},
        {QStringLiteral("suspension"), QStringLiteral("Mecánica"), QStringLiteral("Suspensión"), QStringLiteral("Con fallas")},
        {QStringLiteral("direccion"), QStringLiteral("Mecánica"), QStringLiteral("Dirección"), QStringLiteral("Con fallas")},
        {QStringLiteral("escape"), QStringLiteral("Mecánica"), QStringLiteral("Escape"), QStringLiteral("Con fallas")},
        {QStringLiteral("aire_acondicionado_func"), QStringLiteral("Mecánica"), QStringLiteral("Aire acondicionado"), QStringLiteral("Con fallas")},
    };
    return seeds;
}

// Separación entre ítems consecutivos. Deja huecos para poder intercalar un
// renglón a mano en la base sin tener que renumerar todo el catálogo.
constexpr int kSortOrderStep = 10;

} // namespace

ConditionCatalogSeeder::Result ConditionCatalogSeeder::run(QSqlDatabase &db)
{
    Result result;

    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1").arg(db.lastError().text());
        return result;
    }

    QSqlQuery query(db);
    // DO UPDATE y no DO NOTHING: con DO NOTHING, corregir una etiqueta o el
    // orden de un ítem que ya existe no se propagaría nunca, y cada quien
    // acabaría con un catálogo distinto según cuándo creó su base.
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_conditions_cat "
        "(item_key, category, element, negative_label, sort_order) "
        "VALUES (:item_key, :category, :element, :negative_label, :sort_order) "
        "ON CONFLICT (item_key) DO UPDATE SET "
        "  category = EXCLUDED.category, "
        "  element = EXCLUDED.element, "
        "  negative_label = EXCLUDED.negative_label, "
        "  sort_order = EXCLUDED.sort_order"));

    int sortOrder = 0;
    for (const ConditionSeed &seed : conditionSeeds()) {
        sortOrder += kSortOrderStep;
        query.bindValue(QStringLiteral(":item_key"), seed.itemKey);
        query.bindValue(QStringLiteral(":category"), seed.category);
        query.bindValue(QStringLiteral(":element"), seed.element);
        query.bindValue(QStringLiteral(":negative_label"), seed.negativeLabel);
        query.bindValue(QStringLiteral(":sort_order"), sortOrder);

        if (!query.exec()) {
            // Caso de borde a conocer: si renombras un ítem hacia un
            // (category, element) que ya ocupa otro renglón, el DO UPDATE
            // viola unique_category_element y caes aquí. Es ruidoso, pero es
            // preferible a quedarse con dos filas con el mismo nombre visible.
            result.errorMessage = QStringLiteral("Error sembrando el ítem '%1': %2")
                                       .arg(seed.itemKey, query.lastError().text());
            db.rollback();
            return result;
        }
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1").arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    return result;
}
