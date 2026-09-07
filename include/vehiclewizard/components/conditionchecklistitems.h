#ifndef CONDITIONCHECKLISTITEMS_H
#define CONDITIONCHECKLISTITEMS_H

#include <QList>
#include <QString>

// Definición de un ítem del checklist de condición del Paso 2. El estado
// positivo siempre se guarda/muestra como "Estado óptimo" (mismo valor
// canónico en toda la tabla vehicle_condition_items); negativeStateLabel es
// solo la etiqueta que se muestra en el radio button -- el valor que se
// guarda en BD para el estado negativo siempre es "Con fallas".
struct ConditionChecklistItemDef
{
    QString itemKey;
    QString label;
    QString group;
    QString negativeStateLabel;
};

// Clasificación de los ~34 ítems observados en las capturas de la app legacy
// ("Condición de la Unidad" + "Detalles de Condiciones de la Unidad"),
// agrupados según el wireframe nuevo (Exteriores / Llantas y rines /
// Interiores / Mecánica). Agregar un ítem nuevo es solo añadir una entrada
// aquí -- la UI y el guardado en BD son genéricos sobre esta lista.
inline const QList<ConditionChecklistItemDef> &conditionChecklistItems()
{
    static const QList<ConditionChecklistItemDef> items = {
        // Exteriores
        {"luces_delanteras", "Luces delanteras", "Exteriores", "Con fallas"},
        {"luces_traseras", "Luces traseras", "Exteriores", "Con fallas"},
        {"espejos", "Espejos", "Exteriores", "Con fallas"},
        {"espejo_retrovisor", "Espejo retrovisor", "Exteriores", "Con fallas"},
        {"antena", "Antena", "Exteriores", "Con fallas"},
        {"molduras", "Molduras", "Exteriores", "Con fallas"},
        {"parabrisas", "Parabrisas", "Exteriores", "Con fallas"},
        {"tapon_gasolina", "Tapón de gasolina", "Exteriores", "Con fallas"},
        {"pintura", "Pintura", "Exteriores", "Deteriorada"},
        {"hojalateria", "Hojalatería", "Exteriores", "Deteriorada"},

        // Llantas y rines
        {"llantas", "Llantas", "Llantas y rines", "Gastadas"},
        {"llanta_refaccion", "Llanta de refacción", "Llantas y rines", "Con fallas"},
        {"condicion_rines", "Condición de rines", "Llantas y rines", "Con fallas"},
        {"tapones_llanta", "Tapones de llanta", "Llantas y rines", "Con fallas"},
        {"gato_maneral", "Gato y maneral", "Llantas y rines", "Con fallas"},
        {"llave_rueda", "Llave de rueda", "Llantas y rines", "Con fallas"},
        {"herramientas", "Herramientas", "Llantas y rines", "Con fallas"},
        {"triangulo_seguridad", "Triángulo de seguridad", "Llantas y rines", "Con fallas"},

        // Interiores
        {"interiores", "Interiores", "Interiores", "Deteriorados"},
        {"asientos", "Asientos", "Interiores", "Con fallas"},
        {"tapetes", "Tapetes", "Interiores", "Con fallas"},
        {"alfombras", "Alfombras", "Interiores", "Con fallas"},
        {"manijas", "Manijas", "Interiores", "Con fallas"},
        {"cinturon_seguridad", "Cinturón de seguridad", "Interiores", "Con fallas"},
        {"ceniceros", "Ceniceros", "Interiores", "Con fallas"},
        {"encendedor", "Encendedor", "Interiores", "Con fallas"},
        {"radio", "Radio", "Interiores", "Con fallas"},
        {"bocinas", "Bocinas", "Interiores", "Con fallas"},
        {"instrumentos_tablero", "Instrumentos de tablero", "Interiores", "Con fallas"},
        {"claxon", "Claxon", "Interiores", "Con fallas"},
        {"limpiadores", "Limpiadores", "Interiores", "Con fallas"},

        // Mecánica
        {"motor", "Motor", "Mecánica", "Con fallas"},
        {"frenos", "Frenos", "Mecánica", "Con fallas"},
        {"suspension", "Suspensión", "Mecánica", "Con fallas"},
        {"direccion", "Dirección", "Mecánica", "Con fallas"},
        {"escape", "Escape", "Mecánica", "Con fallas"},
        {"aire_acondicionado_func", "Aire acondicionado", "Mecánica", "Con fallas"},
    };
    return items;
}

// Orden de despliegue de los grupos en el panel derecho del Paso 2.
inline const QList<QString> &conditionChecklistGroups()
{
    static const QList<QString> groups = {
        QStringLiteral("Exteriores"),
        QStringLiteral("Llantas y rines"),
        QStringLiteral("Interiores"),
        QStringLiteral("Mecánica"),
    };
    return groups;
}

#endif // CONDITIONCHECKLISTITEMS_H
