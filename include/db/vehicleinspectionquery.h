#ifndef VEHICLEINSPECTIONQUERY_H
#define VEHICLEINSPECTIONQUERY_H

#include "db/conditioncatalog.h"

#include <QList>
#include <QString>

class QSqlDatabase;

// Un renglón del checklist de condición YA RECONSTRUIDO para un vehículo: el
// ítem del catálogo más lo que la inspección diga de él, si dijo algo.
//
// vehicle_inspection solo guarda los elementos que la unidad trae, así que
// esta capa es la que vuelve a armar la lista completa. Es una proyección de
// lectura, no domain::Inspection: el dominio a propósito no puede representar
// un elemento ausente (ver domain/inspectionitem.h), y aquí hace falta poder,
// porque la pantalla tiene que pintar los renglones vacíos.
struct VehicleInspectionRow
{
    ConditionCatalogItem element;

    // false = la unidad no trae este elemento. Es el renglón que el LEFT JOIN
    // dejó sin pareja, y NO se distingue de "el elemento se agregó al catálogo
    // después de registrar la unidad": las dos cosas se ven igual. Perder esa
    // distinción es el precio de no guardar los renglones desmarcados.
    bool isPresent = false;

    // Solo significan algo cuando isPresent. Con el renglón ausente valen lo
    // que el constructor diga, y leerlos es un error del llamador.
    bool isOptimal = true;
    QString observations;

    bool hasFault() const { return isPresent && !isOptimal; }
};

// Lectura del checklist de condición de UN vehículo.
//
// Va aparte de VehicleRepository por el mismo reparto que ya existe entre
// VehicleInventoryQuery y el repositorio: esto es una proyección de lectura
// sin transacción ni objetos de dominio, aquello es un mapeador de escritura.
//
// Lista vacía + errorMessage lleno  = falló la consulta.
// Lista vacía + errorMessage vacío  = el catálogo está sin sembrar. NO
//                                     significa "vehículo sin inspección":
//                                     una unidad sin un solo elemento marcado
//                                     devuelve el catálogo completo con todos
//                                     los renglones en isPresent = false.
namespace VehicleInspectionQuery
{
// El checklist completo, en el orden del catálogo. Los elementos que faltan
// son los renglones con isPresent == false.
QList<VehicleInspectionRow> load(QSqlDatabase &db, int folio, QString *errorMessage = nullptr);

// Atajo para "¿qué NO trae esta unidad?": el mismo LEFT JOIN, pero filtrando
// en la base en vez de traer las ~37 filas y descartar la mayoría.
QList<ConditionCatalogItem> loadMissing(QSqlDatabase &db, int folio,
                                        QString *errorMessage = nullptr);
} // namespace VehicleInspectionQuery

#endif // VEHICLEINSPECTIONQUERY_H
