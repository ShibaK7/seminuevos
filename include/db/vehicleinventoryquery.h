#ifndef VEHICLEINVENTORYQUERY_H
#define VEHICLEINVENTORYQUERY_H

#include "domain/enums.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

class QSqlDatabase;

// Proyección de LECTURA para la rejilla de inventario: exactamente los campos
// que pinta una tarjeta, ni uno más.
//
// No es un domain::Vehicle recortado ni aspira a serlo. Una unidad completa
// arrastra su Inspection: una fila de vehicle_inspection por cada ítem del
// catálogo que la unidad trae, hasta 37 por unidad, y la tarjeta no muestra
// ninguna. Para cien vehículos podrían ser casi cuatro mil filas traídas para
// pintar cien rectángulos.
//
// Hay una segunda razón, menos obvia y más seria: reconstruir un Vehicle
// obligaría a pasar por VehicleBuilder, que valida. Una unidad vieja que ya no
// cumpla las reglas de hoy desaparecería del listado sin decir nada, y un
// inventario que esconde unidades es peor que uno feo.
struct VehicleSummary
{
    int folio = -1;

    QString brandName;   // vacío si vehicles.brand_id es NULL: la FK lo permite
    QString model;
    int yearModel = 0;

    // nullopt = el texto de la base no pertenece al dominio. statusRaw conserva
    // lo que había, para poder mostrarlo tal cual en vez de inventar un estado.
    std::optional<domain::VehicleStatus> status;
    QString statusRaw;

    // nullopt = la unidad no tiene renglón en NINGUNA de las dos subtablas. No
    // debería ocurrir, porque el guardado es transaccional, pero si ocurre la
    // unidad sigue apareciendo con el precio en blanco en vez de esfumarse.
    std::optional<double> salePrice;

    QString color;
    int mileage = 0;
    QDate addedDate;

    // Ausentes si no existe el renglón de condiciones. La tarjeta pinta un
    // guión y no necesita distinguir por qué falta.
    std::optional<domain::Transmission> transmission;
    // Lo que la tarjeta rotula como "Motor" es el combustible: así lo usa la
    // maqueta de la pantalla, que pone "Diesel" en ese lugar.
    QString fuelTypeName;

    // Ruta RELATIVA a la raíz de almacenamiento, tal como la escribió
    // LocalFileStorageManager. Vacía = sin fotos. Esta capa devuelve rutas,
    // nunca imágenes cargadas: eso es trabajo de la vista.
    QString coverImagePath;

    // "Nissan Kicks 2021", omitiendo las partes que falten.
    QString displayTitle() const;
};

// Criterios de la barra de filtros.
struct VehicleInventoryFilter
{
    // nullopt = "Todos". Se usa optional y NO un miembro VehicleStatus::Todos:
    // agregarlo al enum rompería los switch sin default de enums.cpp, y
    // toDbString() devolvería vacío, que violaría el CHECK al escribir. "Sin
    // filtro" es la ausencia de una elección, no un estado del dominio.
    std::optional<domain::VehicleStatus> status;
    QDate addedFrom;   // inválida = sin cota inferior
    QDate addedTo;     // inválida = sin cota superior
    // Fusible, no paginación: evita que un inventario grande instancie miles de
    // widgets y congele la aplicación.
    int limit = 500;
};

// Lectura del inventario para la rejilla de tarjetas.
//
// Va aparte de VehicleRepository a propósito: esa clase es un mapeador de
// ESCRITURA (una transacción, seis tablas, un visitante para elegir subtabla) y
// esto es una proyección de lectura sin transacción ni objetos de dominio.
//
// Lista vacía + errorMessage lleno  = falló la consulta.
// Lista vacía + errorMessage vacío  = no hay unidades que cumplan los filtros.
// Son mensajes distintos en pantalla, así que el llamador debe distinguirlos.
namespace VehicleInventoryQuery
{
QList<VehicleSummary> load(QSqlDatabase &db, const VehicleInventoryFilter &filter,
                           QString *errorMessage = nullptr);
} // namespace VehicleInventoryQuery

#endif // VEHICLEINVENTORYQUERY_H
