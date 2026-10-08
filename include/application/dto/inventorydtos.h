#ifndef APPLICATION_DTO_INVENTORYDTOS_H
#define APPLICATION_DTO_INVENTORYDTOS_H

#include "domain/value_objects/enums.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace application {

// Criterios de la barra de filtros del inventario.
struct InventoryFilterDto
{
    // nullopt = "Todos". Se usa optional y NO un miembro VehicleStatus::Todos:
    // "sin filtro" es la ausencia de una elección, no un estado del dominio.
    std::optional<domain::VehicleStatus> status;
    QDate addedFrom; // inválida = sin cota inferior
    QDate addedTo;   // inválida = sin cota superior
    // Fusible, no paginación: evita que un inventario grande instancie miles de
    // widgets y congele la aplicación.
    int limit = 500;
};

// Una tarjeta del inventario: exactamente lo que pinta, ni un campo más.
//
// No es un Vehicle recortado. Reconstruir la entidad obligaría a pasar por
// VehicleBuilder, que valida: una unidad vieja que ya no cumpla las reglas de
// hoy desaparecería del listado sin decir nada, y un inventario que esconde
// unidades es peor que uno feo.
struct InventoryItemDto
{
    int folio = -1;
    QString brandName; // vacío si la unidad no tiene marca
    QString model;
    int yearModel = 0;

    // nullopt = el texto de la base no pertenece al dominio; statusRaw
    // conserva lo que había, para mostrarlo tal cual en vez de inventar.
    std::optional<domain::VehicleStatus> status;
    QString statusRaw;

    // nullopt = la unidad no tiene renglón en ninguna subtabla. Aparece con el
    // precio en blanco en vez de esfumarse.
    std::optional<double> salePrice;

    QString color;
    int mileage = 0;
    QDate addedDate;
    std::optional<domain::Transmission> transmission;
    // Lo que la tarjeta rotula como "Motor" es el combustible.
    QString fuelTypeName;

    // Ruta relativa al almacén (vacía = sin fotos) y los bytes de una
    // miniatura de la foto, ya generada fuera del hilo de la interfaz. Vacíos
    // si no hay foto o si ya no está en disco: la vista pone el marcador de
    // posición.
    QString coverImagePath;
    QByteArray coverImage;
};

struct InventoryResultDto
{
    QList<InventoryItemDto> vehicles;
    // Lista vacía + mensaje = falló la consulta; lista vacía sin mensaje = no
    // hay unidades que cumplan los filtros. En pantalla son avisos distintos.
    QString errorMessage;
};

} // namespace application

#endif // APPLICATION_DTO_INVENTORYDTOS_H
