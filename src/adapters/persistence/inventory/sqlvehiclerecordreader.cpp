#include "adapters/persistence/inventory/sqlvehiclerecordreader.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// Índices de columna del SELECT principal. Se nombran porque son casi
// cincuenta: con un query.value(23) suelto, insertar una columna en medio
// rompe el lector sin que nada avise.
enum Column {
    ColFolio = 0,
    ColVersion,
    ColAcquisitionType,
    ColVehicleTypeId,
    ColVehicleTypeName,
    ColSubtypeId,
    ColSubtypeName,
    ColBrandId,
    ColBrandName,
    ColModel,
    ColYearModel,
    ColColor,
    ColMileage,
    ColSerialNumber,
    ColMotorNumber,
    ColPlates,
    ColPlatesHolder,
    ColRepuve,
    ColDescription,
    ColHasDeal,
    ColDealDate,
    ColInvoiceType,
    ColInvoiceNumber,
    ColInvoiceIssuer,
    ColInvoiceFilePath,
    ColPurchasePrice,
    ColPaymentType,
    ColPaymentMethod,
    ColSalePrice,
    ColBasePrice,
    ColCommissionRate,
    ColMaintenanceCost,
    ColObservations,
    ColCounterpartyName,
    ColCounterpartyNationalId,
    ColCounterpartyStreet,
    ColCounterpartySuburb,
    ColCounterpartyLocality,
    ColCounterpartyState,
    ColCounterpartyPostalCode,
    ColCounterpartyVersion,
    ColFuelTypeId,
    ColFuelTypeName,
    ColCylinders,
    ColTransmission,
    ColInteriorMaterial,
    ColWindowRegulators,
    ColAirConditioning,
};

// La unidad con todo lo que es uno a uno: sus catálogos, la subtabla de su
// rama con la contraparte y sus especificaciones. Todo con LEFT JOIN: un
// renglón que falte deja sus columnas en NULL en vez de esconder la unidad.
//
// La subtabla se une solo si es la de la rama de la unidad (la condición va
// en el JOIN): si por un dato corrupto existieran las dos, cuenta la que
// corresponde y no se mezclan. has_deal dice si apareció; sin ella la unidad
// se puede consultar, pero no corregir.
//
// La fecha cae a added_date cuando falta la subtabla, para que el campo no
// muestre la fecha de hoy como si fuera la del trato. La versión es xmin, la
// transacción que escribió por última vez el renglón de la unidad: cambia en
// cada UPDATE, sin agregar columnas al esquema. La contraparte lleva la suya:
// su renglón lo comparten todas las operaciones de esa persona, y otra unidad
// puede corregirlo sin tocar el de esta.
constexpr auto kRecordSelect = R"SQL(
SELECT v.folio,
       CAST(v.xmin AS text) AS row_version,
       v.acquisition_type,
       v.vehicle_type_id, vt.name,
       v.subtype_id,      st.name,
       v.brand_id,        b.name,
       v.model, v.year_model, v.color, v.mileage, v.serial_number, v.motor_number,
       v.plates, v.plates_holder, v.repuve, v.description,
       (acq.vehicle_folio IS NOT NULL OR csg.vehicle_folio IS NOT NULL) AS has_deal,
       COALESCE(acq.acquisition_date, csg.consignment_date, v.added_date) AS deal_date,
       COALESCE(acq.invoice_type, csg.invoice_type),
       COALESCE(acq.invoice_number, csg.invoice_number),
       COALESCE(acq.invoice_issuer, csg.invoice_issuer),
       acq.invoice_file_path,
       acq.purchase_price, acq.payment_type, acq.payment_method, acq.sale_price,
       csg.base_price, csg.commission_rate,
       COALESCE(acq.maintenance_cost, csg.maintenance_cost),
       COALESCE(acq.observations, csg.observations),
       cp.full_name, cp.national_id, cp.street_address, cp.suburb, cp.locality, cp.state,
       cp.postal_code, CAST(cp.xmin AS text) AS counterparty_version,
       cond.fuel_type_id, f.name, cond.cylinders, cond.transmission, cond.interior_material,
       cond.window_regulators, cond.air_conditioning
  FROM vehicles v
  LEFT JOIN vehicle_categories_cat vt   ON vt.id   = v.vehicle_type_id
  LEFT JOIN vehicle_categories_cat st   ON st.id   = v.subtype_id
  LEFT JOIN brands_cat             b    ON b.id    = v.brand_id
  LEFT JOIN vehicle_acquisitions   acq  ON acq.vehicle_folio = v.folio
                                       AND v.acquisition_type = :acquisition
  LEFT JOIN vehicle_consignments   csg  ON csg.vehicle_folio = v.folio
                                       AND v.acquisition_type = :consignment
  LEFT JOIN counterparties         cp   ON cp.id   = COALESCE(acq.seller_id, csg.owner_id)
  LEFT JOIN vehicle_conditions     cond ON cond.vehicle_folio = v.folio
  LEFT JOIN fuel_type_cat          f    ON f.id    = cond.fuel_type_id
 WHERE v.folio = :folio
)SQL";

domain::CatalogRef catalogRef(const QVariant &id, const QVariant &name)
{
    domain::CatalogRef ref;
    if (!id.isNull()) {
        ref.id = id.toInt();
        ref.name = name.toString();
    }
    return ref;
}

// Un opcional que no mapea queda vacío y su combo pide elegir otra vez; se
// avisa en el log porque es un dato que el CHECK de la base no debió dejar
// pasar.
template <typename Enum>
std::optional<Enum> optionalEnum(const QVariant &raw, std::optional<Enum> (*fromDb)(const QString &),
                                 int folio, const char *column)
{
    const QString text = raw.toString();
    if (text.isEmpty())
        return std::nullopt;
    const std::optional<Enum> value = fromDb(text);
    if (!value) {
        qWarning("Vehiculo %d: %s '%s' no pertenece al dominio.", folio, column,
                 qUtf8Printable(text));
    }
    return value;
}

// Prepara y ejecuta una consulta del folio. Devuelve false con el error.
bool execForFolio(QSqlQuery &query, const QString &sql, int folio, QString &error)
{
    query.prepare(sql);
    query.bindValue(QStringLiteral(":folio"), folio);
    if (!query.exec()) {
        error = query.lastError().text();
        return false;
    }
    return true;
}

} // namespace

SqlVehicleRecordReader::SqlVehicleRecordReader(ConnectionPool &pool)
    : m_pool(pool)
{
}

std::optional<application::VehicleRecordDto> SqlVehicleRecordReader::findByFolio(int folio,
                                                                               QString *error)
{
    std::optional<application::VehicleRecordDto> record;
    QString failure;
    bool connectionBroken = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = QStringLiteral("No hay conexión con la base de datos: %1")
                          .arg(db.lastError().text());
        } else if (!db.transaction()) {
            failure = QStringLiteral("No se pudo iniciar la lectura: %1").arg(db.lastError().text());
        } else {
            record = read(db, folio, failure);
            // Solo lectura: confirmar o deshacer da lo mismo, lo que importa es
            // cerrar la transacción. Si ni eso se pudo, la conexión quedó en un
            // estado que no sirve para la siguiente lectura.
            if (failure.isEmpty())
                connectionBroken = !db.commit();
            else
                db.rollback();
        }
    }
    // Con las consultas y el Handle ya destruidos: si PostgreSQL se reinició,
    // la conexión de este hilo quedó inservible y la siguiente lectura debe
    // abrir otra.
    if (!failure.isEmpty() || connectionBroken)
        m_pool.discardThreadConnection();
    if (!failure.isEmpty()) {
        if (error)
            *error = failure;
        return std::nullopt;
    }
    return record;
}

std::optional<application::VehicleRecordDto> SqlVehicleRecordReader::read(QSqlDatabase &db,
                                                                          int folio,
                                                                          QString &error)
{
    // Tiene que ser lo primero de la transacción: fija la foto de la base que
    // verán las cuatro consultas.
    QSqlQuery isolation(db);
    if (!isolation.exec(
            QStringLiteral("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY"))) {
        error = isolation.lastError().text();
        return std::nullopt;
    }

    QSqlQuery query(db);
    query.prepare(QString::fromLatin1(kRecordSelect));
    query.bindValue(QStringLiteral(":acquisition"),
                    domain::toDbString(domain::AcquisitionType::Adquisicion));
    query.bindValue(QStringLiteral(":consignment"),
                    domain::toDbString(domain::AcquisitionType::Consignacion));
    query.bindValue(QStringLiteral(":folio"), folio);
    if (!query.exec()) {
        error = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt; // no existe

    application::VehicleRecordDto record;
    record.folio = query.value(ColFolio).toInt();
    record.version = query.value(ColVersion).toString();

    // La rama decide qué subtabla se lee y qué factura admite su combo: una
    // que no mapea no se puede pintar sin inventarla.
    const QString branch = query.value(ColAcquisitionType).toString();
    const std::optional<domain::AcquisitionType> type = domain::acquisitionTypeFromDb(branch);
    if (!type) {
        error = QStringLiteral("La unidad %1 tiene un tipo de operación desconocido (%2).")
                    .arg(folio)
                    .arg(branch);
        return std::nullopt;
    }

    application::VehicleDetailsDto &d = record.details;
    d.acquisitionType = *type;
    d.vehicleType = catalogRef(query.value(ColVehicleTypeId), query.value(ColVehicleTypeName));
    d.subtype = catalogRef(query.value(ColSubtypeId), query.value(ColSubtypeName));
    d.brand = catalogRef(query.value(ColBrandId), query.value(ColBrandName));
    d.model = query.value(ColModel).toString();
    d.yearModel = query.value(ColYearModel).toInt();
    d.color = query.value(ColColor).toString();
    d.mileage = query.value(ColMileage).toInt();
    d.serialNumber = query.value(ColSerialNumber).toString();
    d.motorNumber = query.value(ColMotorNumber).toString();
    d.plates = query.value(ColPlates).toString();
    d.platesHolder = query.value(ColPlatesHolder).toString();
    d.repuve = query.value(ColRepuve).toString();
    d.description = query.value(ColDescription).toString();

    record.hasDealTerms = query.value(ColHasDeal).toBool();
    d.dealDate = query.value(ColDealDate).toDate();
    d.invoiceType = optionalEnum(query.value(ColInvoiceType), &domain::invoiceTypeFromDb, folio,
                                 "invoice_type");
    d.invoiceNumber = query.value(ColInvoiceNumber).toString();
    d.invoiceIssuer = query.value(ColInvoiceIssuer).toString();
    d.invoiceFilePath = query.value(ColInvoiceFilePath).toString();
    d.purchasePrice = query.value(ColPurchasePrice).toDouble();
    // Tipo y método de pago no son opcionales en el DTO: si no mapean, se
    // quedan con su valor por omisión (y el aviso en el log).
    if (const auto payment = optionalEnum(query.value(ColPaymentType), &domain::paymentTypeFromDb,
                                          folio, "payment_type"))
        d.paymentType = *payment;
    if (const auto method = optionalEnum(query.value(ColPaymentMethod),
                                         &domain::paymentMethodFromDb, folio, "payment_method"))
        d.paymentMethod = *method;
    d.salePrice = query.value(ColSalePrice).toDouble();
    d.basePrice = query.value(ColBasePrice).toDouble();
    d.commissionRate = query.value(ColCommissionRate).toDouble();
    d.maintenanceCost = query.value(ColMaintenanceCost).toDouble();
    d.observations = query.value(ColObservations).toString();

    d.counterparty.fullName = query.value(ColCounterpartyName).toString();
    d.counterparty.nationalId = query.value(ColCounterpartyNationalId).toString();
    d.counterparty.streetAddress = query.value(ColCounterpartyStreet).toString();
    d.counterparty.suburb = query.value(ColCounterpartySuburb).toString();
    d.counterparty.locality = query.value(ColCounterpartyLocality).toString();
    d.counterparty.state = query.value(ColCounterpartyState).toString();
    d.counterparty.postalCode = query.value(ColCounterpartyPostalCode).toString();
    record.counterpartyVersion = query.value(ColCounterpartyVersion).toString();

    application::VehicleConditionsDto &c = record.conditions;
    c.fuelType = catalogRef(query.value(ColFuelTypeId), query.value(ColFuelTypeName));
    if (!query.value(ColCylinders).isNull())
        c.cylinders = query.value(ColCylinders).toInt();
    c.transmission = optionalEnum(query.value(ColTransmission), &domain::transmissionFromDb, folio,
                                  "transmission");
    c.interiorMaterial = query.value(ColInteriorMaterial).toString();
    c.windowRegulators = optionalEnum(query.value(ColWindowRegulators),
                                      &domain::windowRegulatorsFromDb, folio, "window_regulators");
    c.airConditioning = optionalEnum(query.value(ColAirConditioning),
                                     &domain::airConditioningFromDb, folio, "air_conditioning");

    // Solo los elementos que la unidad trae: la ausencia de renglón ya dice que
    // no lo trae.
    QSqlQuery inspection(db);
    if (!execForFolio(inspection,
                      QStringLiteral("SELECT element_id, is_optimal, observations "
                                     "FROM vehicle_inspection WHERE vehicle_folio = :folio "
                                     "ORDER BY element_id"),
                      folio, error))
        return std::nullopt;
    while (inspection.next()) {
        application::InspectionEntryDto entry;
        entry.elementId = inspection.value(0).toInt();
        // is_optimal es nulable con TRUE por omisión: NULL cuenta como óptimo.
        entry.isOptimal = inspection.value(1).isNull() || inspection.value(1).toBool();
        entry.observations = inspection.value(2).toString();
        c.inspection << entry;
    }

    QSqlQuery images(db);
    if (!execForFolio(images,
                      QStringLiteral("SELECT file_path, is_primary FROM vehicle_images "
                                     "WHERE vehicle_folio = :folio ORDER BY id"),
                      folio, error))
        return std::nullopt;
    while (images.next()) {
        application::StoredImageDto stored;
        stored.image.path = images.value(0).toString();
        stored.image.isPrimary = images.value(1).toBool();
        record.images << stored;
    }

    // Solo los que tienen archivo: un renglón sin ruta no es algo que el
    // expediente capture, y al guardar se deja como está (el UPDATE usa el
    // mismo filtro).
    QSqlQuery documents(db);
    if (!execForFolio(documents,
                      QStringLiteral("SELECT document_type, file_path, document_number "
                                     "FROM vehicle_documents "
                                     "WHERE vehicle_folio = :folio AND file_path IS NOT NULL "
                                     "ORDER BY id"),
                      folio, error))
        return std::nullopt;
    while (documents.next()) {
        record.documents << domain::VehicleDocument{documents.value(0).toString(),
                                                    documents.value(1).toString(),
                                                    documents.value(2).toString()};
    }

    return record;
}
