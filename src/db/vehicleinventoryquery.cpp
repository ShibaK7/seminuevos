#include "../../include/db/vehicleinventoryquery.h"

#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace {

// Índices de columna del SELECT. Se nombran porque son trece: con un
// query.value(9) suelto, insertar una columna en medio rompe el lector sin que
// nada avise.
enum Column {
    ColFolio = 0,
    ColStatus,
    ColBrandName,
    ColModel,
    ColYearModel,
    ColColor,
    ColMileage,
    ColAddedDate,
    ColSalePrice,
    ColTransmission,
    ColFuelTypeName,
    ColCoverImagePath,
};

// El precio vive en una subtabla u otra según la rama, así que se hace LEFT
// JOIN a las dos y se toma la que haya. No hay riesgo de multiplicar filas:
// vehicle_folio es llave primaria en ambas, de modo que cada JOIN devuelve a lo
// sumo un renglón. Con UNION de dos consultas, en cambio, una unidad sin
// renglón de subtabla desaparecería del listado; así aparece con el precio en
// blanco, que es justo lo que conviene ver si algo se rompió.
//
// La portada va por subconsulta y no por JOIN porque no hay UNIQUE que impida
// dos filas marcadas como principal, y con JOIN esa unidad saldría DUPLICADA en
// la rejilla. El ORDER BY interno además degrada con gracia: si no hay ninguna
// marcada, toma la primera foto en vez de dejar el hueco.
//
// NULLS LAST no es adorno: is_primary y added_date son nulables, y bajo DESC
// PostgreSQL pone los NULL primero. Sin eso, una fila con is_primary NULL le
// ganaría a la portada de verdad.
constexpr auto kBaseSelect = R"SQL(
SELECT v.folio,
       v.status,
       b.name  AS brand_name,
       v.model,
       v.year_model,
       v.color,
       v.mileage,
       v.added_date,
       COALESCE(acq.sale_price, csg.sale_price) AS sale_price,
       cond.transmission,
       f.name  AS fuel_type_name,
       (SELECT img.file_path
          FROM vehicle_images img
         WHERE img.vehicle_folio = v.folio
         ORDER BY img.is_primary DESC NULLS LAST, img.id
         LIMIT 1) AS cover_image_path
  FROM vehicles v
  LEFT JOIN brands_cat           b    ON b.id               = v.brand_id
  LEFT JOIN vehicle_acquisitions acq  ON acq.vehicle_folio  = v.folio
  LEFT JOIN vehicle_consignments csg  ON csg.vehicle_folio  = v.folio
  LEFT JOIN vehicle_conditions   cond ON cond.vehicle_folio = v.folio
  LEFT JOIN fuel_type_cat        f    ON f.id               = cond.fuel_type_id
)SQL";

} // namespace

QString VehicleSummary::displayTitle() const
{
    QStringList parts;
    if (!brandName.isEmpty())
        parts << brandName;
    if (!model.isEmpty())
        parts << model;
    if (yearModel > 0)
        parts << QString::number(yearModel);
    return parts.join(QLatin1Char(' '));
}

QList<VehicleSummary> VehicleInventoryQuery::load(QSqlDatabase &db,
                                                  const VehicleInventoryFilter &filter,
                                                  QString *errorMessage)
{
    QList<VehicleSummary> rows;

    // Dos listas en paralelo: el fragmento de SQL, que siempre es un literal
    // escrito aquí, y el valor, que viene del usuario y viaja por bindValue.
    // Ningún byte capturado en pantalla toca la cadena de la consulta.
    QStringList clauses;
    QVariantMap binds;

    if (filter.status) {
        clauses << QStringLiteral("v.status = :status");
        binds.insert(QStringLiteral(":status"), domain::toDbString(*filter.status));
    }
    // added_date es DATE, no TIMESTAMP, así que el rango es inclusivo en los dos
    // extremos y no hay que sumarle un día a la cota superior.
    if (filter.addedFrom.isValid()) {
        clauses << QStringLiteral("v.added_date >= :added_from");
        binds.insert(QStringLiteral(":added_from"), filter.addedFrom);
    }
    if (filter.addedTo.isValid()) {
        clauses << QStringLiteral("v.added_date <= :added_to");
        binds.insert(QStringLiteral(":added_to"), filter.addedTo);
    }

    QString sql = QString::fromLatin1(kBaseSelect);
    if (!clauses.isEmpty())
        sql += QStringLiteral("\n WHERE ") + clauses.join(QStringLiteral("\n   AND "));
    // El desempate por folio importa: added_date es una fecha sin hora, así que
    // muchas unidades la comparten. Sin él, su orden relativo es indefinido y la
    // rejilla se reacomoda sola entre arranques.
    sql += QStringLiteral("\n ORDER BY v.added_date DESC NULLS LAST, v.folio DESC"
                          "\n LIMIT :limit");

    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return rows;
    }
    for (auto it = binds.cbegin(); it != binds.cend(); ++it)
        query.bindValue(it.key(), it.value());
    query.bindValue(QStringLiteral(":limit"), qMax(1, filter.limit));

    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return rows;
    }

    while (query.next()) {
        VehicleSummary summary;
        summary.folio = query.value(ColFolio).toInt();

        // Un estado que no mapea NO descarta el renglón ni se sustituye por uno
        // inventado: la unidad aparece con su texto crudo. Ocultarla la haría
        // desaparecer del inventario sin explicación, y darla por "Disponible"
        // podría llevar a que alguien intente vender dos veces la misma unidad.
        summary.statusRaw = query.value(ColStatus).toString();
        summary.status = domain::vehicleStatusFromDb(summary.statusRaw);
        if (!summary.status && !summary.statusRaw.isEmpty()) {
            qWarning("Vehiculo %d: el estado '%s' no pertenece al dominio.", summary.folio,
                     qUtf8Printable(summary.statusRaw));
        }

        summary.brandName = query.value(ColBrandName).toString();
        summary.model = query.value(ColModel).toString();
        summary.yearModel = query.value(ColYearModel).toInt();
        summary.color = query.value(ColColor).toString();
        summary.mileage = query.value(ColMileage).toInt();
        summary.addedDate = query.value(ColAddedDate).toDate();

        const QVariant price = query.value(ColSalePrice);
        if (!price.isNull())
            summary.salePrice = price.toDouble();

        const QString transmission = query.value(ColTransmission).toString();
        if (!transmission.isEmpty())
            summary.transmission = domain::transmissionFromDb(transmission);

        summary.fuelTypeName = query.value(ColFuelTypeName).toString();
        summary.coverImagePath = query.value(ColCoverImagePath).toString();

        rows << summary;
    }

    return rows;
}
