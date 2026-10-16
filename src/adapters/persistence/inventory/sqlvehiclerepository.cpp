#include "adapters/persistence/inventory/sqlvehiclerepository.h"

#include "adapters/persistence/connectionpool.h"
#include "adapters/persistence/inventory/sqlcounterpartyrepository.h"
#include "domain/inventory/model/acquisition/acquiredvehicle.h"
#include "domain/inventory/model/consignment/consignedvehicle.h"
#include "domain/inventory/model/vehicle.h"
#include "domain/inventory/model/vehiclevisitor.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <optional>

namespace {

QVariant nullIfEmpty(const QString &value)
{
    if (value.isEmpty())
        return QVariant(QMetaType(QMetaType::QString));
    return value;
}

// Transmisión, cristales y aire acondicionado son opcionales en el dominio,
// pero aquí llegan ya validados: build() no entrega una unidad a la que le
// falte alguno, así que se usa el valor capturado. Si aun así faltara, se
// manda NULL y el NOT NULL de la columna lo rechaza con un error claro, en
// vez de leer un optional vacío.
template <typename Enum>
QVariant enumOrNull(const std::optional<Enum> &value)
{
    if (!value)
        return QVariant(QMetaType(QMetaType::QString));
    return domain::toDbString(*value);
}

// Una referencia de catálogo sin elegir tiene que llegar como NULL, no como
// cero: un cero violaría la llave foránea contra la tabla de catálogo.
QVariant catalogId(const domain::CatalogRef &ref)
{
    if (!ref.isValid())
        return QVariant(QMetaType(QMetaType::Int));
    return ref.id;
}

// Escribe la subtabla que corresponde a la rama. Es la mitad de un doble
// despacho: el vehículo sabe qué es y llama al visit() correcto, y el
// visitante sabe qué SQL escribir sin preguntar por el tipo.
class DealInserter final : public domain::VehicleVisitor
{
public:
    DealInserter(QSqlDatabase &db, int folio, int counterpartyId)
        : m_db(db)
        , m_folio(folio)
        , m_counterpartyId(counterpartyId)
    {
    }

    bool ok() const { return m_ok; }
    const QString &errorMessage() const { return m_errorMessage; }

    void visit(const domain::AcquiredVehicle &vehicle) override
    {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "INSERT INTO vehicle_acquisitions "
            "(vehicle_folio, seller_id, invoice_type, invoice_number, invoice_issuer, "
            " invoice_file_path, purchase_price, payment_type, payment_method, "
            " maintenance_cost, sale_price, observations, acquisition_date) "
            "VALUES (:vehicle_folio, :seller_id, :invoice_type, :invoice_number, :invoice_issuer, "
            " :invoice_file_path, :purchase_price, :payment_type, :payment_method, "
            " :maintenance_cost, :sale_price, :observations, :acquisition_date)"));
        query.bindValue(QStringLiteral(":vehicle_folio"), m_folio);
        query.bindValue(QStringLiteral(":seller_id"), m_counterpartyId);
        query.bindValue(QStringLiteral(":invoice_type"), domain::toDbString(vehicle.invoiceType()));
        query.bindValue(QStringLiteral(":invoice_number"), nullIfEmpty(vehicle.invoiceNumber()));
        query.bindValue(QStringLiteral(":invoice_issuer"), nullIfEmpty(vehicle.invoiceIssuer()));
        query.bindValue(QStringLiteral(":invoice_file_path"), nullIfEmpty(vehicle.invoiceFilePath()));
        query.bindValue(QStringLiteral(":purchase_price"), vehicle.purchasePrice());
        query.bindValue(QStringLiteral(":payment_type"), domain::toDbString(vehicle.paymentType()));
        query.bindValue(QStringLiteral(":payment_method"), domain::toDbString(vehicle.paymentMethod()));
        query.bindValue(QStringLiteral(":maintenance_cost"), vehicle.maintenanceCost());
        query.bindValue(QStringLiteral(":sale_price"), vehicle.salePrice());
        query.bindValue(QStringLiteral(":observations"), nullIfEmpty(vehicle.observations()));
        query.bindValue(QStringLiteral(":acquisition_date"), vehicle.dealDate());

        run(query);
    }

    void visit(const domain::ConsignedVehicle &vehicle) override
    {
        // sale_price NO va en la lista de columnas: en vehicle_consignments es
        // GENERATED ALWAYS, y mencionarla hace que PostgreSQL rechace el
        // INSERT con "cannot insert a non-DEFAULT value into column". El valor
        // que devuelve ConsignedVehicle::salePrice() replica la misma fórmula
        // con el mismo redondeo a centavos, para mostrarlo antes de guardar.
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "INSERT INTO vehicle_consignments "
            "(vehicle_folio, owner_id, invoice_type, invoice_number, invoice_issuer, "
            " base_price, commission_rate, maintenance_cost, observations, consignment_date) "
            "VALUES (:vehicle_folio, :owner_id, :invoice_type, :invoice_number, :invoice_issuer, "
            " :base_price, :commission_rate, :maintenance_cost, :observations, :consignment_date)"));
        query.bindValue(QStringLiteral(":vehicle_folio"), m_folio);
        query.bindValue(QStringLiteral(":owner_id"), m_counterpartyId);
        query.bindValue(QStringLiteral(":invoice_type"), domain::toDbString(vehicle.invoiceType()));
        query.bindValue(QStringLiteral(":invoice_number"), nullIfEmpty(vehicle.invoiceNumber()));
        query.bindValue(QStringLiteral(":invoice_issuer"), nullIfEmpty(vehicle.invoiceIssuer()));
        query.bindValue(QStringLiteral(":base_price"), vehicle.basePrice());
        query.bindValue(QStringLiteral(":commission_rate"), vehicle.commissionRate());
        query.bindValue(QStringLiteral(":maintenance_cost"), vehicle.maintenanceCost());
        query.bindValue(QStringLiteral(":observations"), nullIfEmpty(vehicle.observations()));
        query.bindValue(QStringLiteral(":consignment_date"), vehicle.dealDate());

        run(query);
    }

private:
    void run(QSqlQuery &query)
    {
        if (!query.exec()) {
            m_ok = false;
            m_errorMessage = query.lastError().text();
        }
    }

    QSqlDatabase &m_db;
    int m_folio;
    int m_counterpartyId;
    bool m_ok = true;
    QString m_errorMessage;
};

// El gemelo de DealInserter para corregir: reescribe la subtabla de la rama.
// Nunca toca maintenance_cost, que acumula el historial de mantenimientos
// (otro usuario puede estar sumándole mientras esta corrección se captura), ni
// el sale_price de una consignación, que la base calcula.
class DealUpdater final : public domain::VehicleVisitor
{
public:
    DealUpdater(QSqlDatabase &db, int folio, int counterpartyId)
        : m_db(db)
        , m_folio(folio)
        , m_counterpartyId(counterpartyId)
    {
    }

    bool ok() const { return m_ok; }
    const QString &errorMessage() const { return m_errorMessage; }

    void visit(const domain::AcquiredVehicle &vehicle) override
    {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "UPDATE vehicle_acquisitions "
            "   SET seller_id = :seller_id, invoice_type = :invoice_type, "
            "       invoice_number = :invoice_number, invoice_issuer = :invoice_issuer, "
            "       invoice_file_path = :invoice_file_path, purchase_price = :purchase_price, "
            "       payment_type = :payment_type, payment_method = :payment_method, "
            "       sale_price = :sale_price, observations = :observations, "
            "       acquisition_date = :acquisition_date "
            " WHERE vehicle_folio = :vehicle_folio"));
        query.bindValue(QStringLiteral(":seller_id"), m_counterpartyId);
        query.bindValue(QStringLiteral(":invoice_type"), domain::toDbString(vehicle.invoiceType()));
        query.bindValue(QStringLiteral(":invoice_number"), nullIfEmpty(vehicle.invoiceNumber()));
        query.bindValue(QStringLiteral(":invoice_issuer"), nullIfEmpty(vehicle.invoiceIssuer()));
        query.bindValue(QStringLiteral(":invoice_file_path"), nullIfEmpty(vehicle.invoiceFilePath()));
        query.bindValue(QStringLiteral(":purchase_price"), vehicle.purchasePrice());
        query.bindValue(QStringLiteral(":payment_type"), domain::toDbString(vehicle.paymentType()));
        query.bindValue(QStringLiteral(":payment_method"), domain::toDbString(vehicle.paymentMethod()));
        query.bindValue(QStringLiteral(":sale_price"), vehicle.salePrice());
        query.bindValue(QStringLiteral(":observations"), nullIfEmpty(vehicle.observations()));
        query.bindValue(QStringLiteral(":acquisition_date"), vehicle.dealDate());
        query.bindValue(QStringLiteral(":vehicle_folio"), m_folio);

        run(query);
    }

    void visit(const domain::ConsignedVehicle &vehicle) override
    {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "UPDATE vehicle_consignments "
            "   SET owner_id = :owner_id, invoice_type = :invoice_type, "
            "       invoice_number = :invoice_number, invoice_issuer = :invoice_issuer, "
            "       base_price = :base_price, commission_rate = :commission_rate, "
            "       observations = :observations, consignment_date = :consignment_date "
            " WHERE vehicle_folio = :vehicle_folio"));
        query.bindValue(QStringLiteral(":owner_id"), m_counterpartyId);
        query.bindValue(QStringLiteral(":invoice_type"), domain::toDbString(vehicle.invoiceType()));
        query.bindValue(QStringLiteral(":invoice_number"), nullIfEmpty(vehicle.invoiceNumber()));
        query.bindValue(QStringLiteral(":invoice_issuer"), nullIfEmpty(vehicle.invoiceIssuer()));
        query.bindValue(QStringLiteral(":base_price"), vehicle.basePrice());
        query.bindValue(QStringLiteral(":commission_rate"), vehicle.commissionRate());
        query.bindValue(QStringLiteral(":observations"), nullIfEmpty(vehicle.observations()));
        query.bindValue(QStringLiteral(":consignment_date"), vehicle.dealDate());
        query.bindValue(QStringLiteral(":vehicle_folio"), m_folio);

        run(query);
    }

private:
    // Exactamente un renglón: si la subtabla no tiene el de esta unidad, la
    // corrección no tiene qué corregir y se deshace completa.
    void run(QSqlQuery &query)
    {
        if (!query.exec()) {
            m_ok = false;
            m_errorMessage = query.lastError().text();
        } else if (query.numRowsAffected() != 1) {
            m_ok = false;
            m_errorMessage = QStringLiteral("No se encontró la operación de la unidad %1.")
                                 .arg(m_folio);
        }
    }

    QSqlDatabase &m_db;
    int m_folio;
    int m_counterpartyId;
    bool m_ok = true;
    QString m_errorMessage;
};

// Corre una consulta que solo recibe el folio.
bool execForFolio(QSqlDatabase &db, const QString &sql, int folio, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":folio"), folio);
    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

// La contraparte de la operación de la unidad, con el mismo discriminador de
// rama que la lectura del expediente. -1 si no hay operación o si falla.
int currentCounterpartyId(QSqlDatabase &db, int folio, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT COALESCE(acq.seller_id, csg.owner_id) "
        "  FROM vehicles v "
        "  LEFT JOIN vehicle_acquisitions acq ON acq.vehicle_folio = v.folio "
        "                                    AND v.acquisition_type = :acquisition "
        "  LEFT JOIN vehicle_consignments csg ON csg.vehicle_folio = v.folio "
        "                                    AND v.acquisition_type = :consignment "
        " WHERE v.folio = :folio"));
    query.bindValue(QStringLiteral(":acquisition"),
                    domain::toDbString(domain::AcquisitionType::Adquisicion));
    query.bindValue(QStringLiteral(":consignment"),
                    domain::toDbString(domain::AcquisitionType::Consignacion));
    query.bindValue(QStringLiteral(":folio"), folio);
    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return -1;
    }
    if (!query.next() || query.value(0).isNull()) {
        errorMessage = QStringLiteral("La unidad %1 no tiene registrada su operación.").arg(folio);
        return -1;
    }
    return query.value(0).toInt();
}

int insertVehicleRow(QSqlDatabase &db, const domain::Vehicle &vehicle, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicles "
        "(acquisition_type, status, vehicle_type_id, subtype_id, brand_id, model, year_model, "
        " color, mileage, serial_number, motor_number, plates, plates_holder, repuve, "
        " description, added_date) "
        "VALUES (:acquisition_type, :status, :vehicle_type_id, :subtype_id, :brand_id, :model, "
        " :year_model, :color, :mileage, :serial_number, :motor_number, :plates, :plates_holder, "
        " :repuve, :description, :added_date) "
        "RETURNING folio"));
    // El discriminador sale del propio objeto. Antes estaba escrito a mano
    // dentro de la cadena SQL, que es lo que hacía imposible registrar una
    // consignación.
    query.bindValue(QStringLiteral(":acquisition_type"), domain::toDbString(vehicle.acquisitionType()));
    query.bindValue(QStringLiteral(":status"), domain::toDbString(vehicle.status()));
    query.bindValue(QStringLiteral(":vehicle_type_id"), catalogId(vehicle.vehicleType()));
    query.bindValue(QStringLiteral(":subtype_id"), catalogId(vehicle.subtype()));
    query.bindValue(QStringLiteral(":brand_id"), catalogId(vehicle.brand()));
    query.bindValue(QStringLiteral(":model"), vehicle.model());
    query.bindValue(QStringLiteral(":year_model"), vehicle.yearModel());
    query.bindValue(QStringLiteral(":color"), nullIfEmpty(vehicle.color()));
    query.bindValue(QStringLiteral(":mileage"), vehicle.mileage());
    query.bindValue(QStringLiteral(":serial_number"), nullIfEmpty(vehicle.serialNumber()));
    query.bindValue(QStringLiteral(":motor_number"), nullIfEmpty(vehicle.motorNumber()));
    query.bindValue(QStringLiteral(":plates"), nullIfEmpty(vehicle.plates()));
    query.bindValue(QStringLiteral(":plates_holder"), nullIfEmpty(vehicle.platesHolder()));
    query.bindValue(QStringLiteral(":repuve"), nullIfEmpty(vehicle.repuve()));
    query.bindValue(QStringLiteral(":description"), nullIfEmpty(vehicle.description()));
    query.bindValue(QStringLiteral(":added_date"), vehicle.addedDate());

    if (!query.exec() || !query.next()) {
        errorMessage = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}

// La unidad corregida, solo si nadie la cambió desde que se leyó: la versión
// es xmin, que cambia con cada UPDATE del renglón. Nunca toca la rama ni el
// estado; la fecha de ingreso sigue a la del trato, como en el alta.
bool updateVehicleRow(QSqlDatabase &db, const domain::Vehicle &vehicle,
                      const QString &expectedVersion, int &rowsAffected, QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE vehicles "
        "   SET vehicle_type_id = :vehicle_type_id, subtype_id = :subtype_id, "
        "       brand_id = :brand_id, model = :model, year_model = :year_model, color = :color, "
        "       mileage = :mileage, serial_number = :serial_number, "
        "       motor_number = :motor_number, plates = :plates, plates_holder = :plates_holder, "
        "       repuve = :repuve, description = :description, added_date = :added_date "
        " WHERE folio = :folio AND acquisition_type = :acquisition_type "
        "   AND CAST(xmin AS text) = :version"));
    query.bindValue(QStringLiteral(":vehicle_type_id"), catalogId(vehicle.vehicleType()));
    query.bindValue(QStringLiteral(":subtype_id"), catalogId(vehicle.subtype()));
    query.bindValue(QStringLiteral(":brand_id"), catalogId(vehicle.brand()));
    query.bindValue(QStringLiteral(":model"), vehicle.model());
    query.bindValue(QStringLiteral(":year_model"), vehicle.yearModel());
    query.bindValue(QStringLiteral(":color"), nullIfEmpty(vehicle.color()));
    query.bindValue(QStringLiteral(":mileage"), vehicle.mileage());
    query.bindValue(QStringLiteral(":serial_number"), nullIfEmpty(vehicle.serialNumber()));
    query.bindValue(QStringLiteral(":motor_number"), nullIfEmpty(vehicle.motorNumber()));
    query.bindValue(QStringLiteral(":plates"), nullIfEmpty(vehicle.plates()));
    query.bindValue(QStringLiteral(":plates_holder"), nullIfEmpty(vehicle.platesHolder()));
    query.bindValue(QStringLiteral(":repuve"), nullIfEmpty(vehicle.repuve()));
    query.bindValue(QStringLiteral(":description"), nullIfEmpty(vehicle.description()));
    query.bindValue(QStringLiteral(":added_date"), vehicle.addedDate());
    query.bindValue(QStringLiteral(":folio"), vehicle.folio());
    query.bindValue(QStringLiteral(":acquisition_type"), domain::toDbString(vehicle.acquisitionType()));
    query.bindValue(QStringLiteral(":version"), expectedVersion);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    rowsAffected = query.numRowsAffected();
    return true;
}

// upsert: al corregir, el renglón ya existe (o, en datos viejos, puede
// faltar), así que se escribe encima en vez de fallar.
bool insertConditions(QSqlDatabase &db, int folio, const domain::VehicleConditions &conditions,
                      QString &errorMessage, bool upsert = false)
{
    QString sql = QStringLiteral(
        "INSERT INTO vehicle_conditions "
        "(vehicle_folio, fuel_type_id, cylinders, transmission, interior_material, "
        " window_regulators, air_conditioning) "
        "VALUES (:vehicle_folio, :fuel_type_id, :cylinders, :transmission, :interior_material, "
        " :window_regulators, :air_conditioning)");
    if (upsert) {
        sql += QStringLiteral(
            " ON CONFLICT (vehicle_folio) DO UPDATE "
            "   SET fuel_type_id = EXCLUDED.fuel_type_id, cylinders = EXCLUDED.cylinders, "
            "       transmission = EXCLUDED.transmission, "
            "       interior_material = EXCLUDED.interior_material, "
            "       window_regulators = EXCLUDED.window_regulators, "
            "       air_conditioning = EXCLUDED.air_conditioning, "
            "       updated_at = CURRENT_TIMESTAMP");
    }
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":vehicle_folio"), folio);
    query.bindValue(QStringLiteral(":fuel_type_id"), catalogId(conditions.fuelType()));
    query.bindValue(QStringLiteral(":cylinders"), conditions.cylinders());
    query.bindValue(QStringLiteral(":transmission"), enumOrNull(conditions.transmission()));
    query.bindValue(QStringLiteral(":interior_material"), nullIfEmpty(conditions.interiorMaterial()));
    query.bindValue(QStringLiteral(":window_regulators"), enumOrNull(conditions.windowRegulators()));
    query.bindValue(QStringLiteral(":air_conditioning"), enumOrNull(conditions.airConditioning()));

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool insertInspection(QSqlDatabase &db, int folio, const domain::Inspection &inspection,
                      QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_inspection "
        "(vehicle_folio, element_id, is_optimal, observations) "
        "VALUES (:vehicle_folio, :element_id, :is_optimal, :observations)"));

    // Solo llegan aquí los elementos que la unidad trae, así que se insertan
    // todos: no hay nada que filtrar. Una inspección vacía no inserta nada, y
    // eso es correcto -- significa que la unidad no trae ninguno.
    for (const domain::InspectionItem &item : inspection.items()) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":element_id"), item.elementId());
        query.bindValue(QStringLiteral(":is_optimal"), item.isOptimal());
        query.bindValue(QStringLiteral(":observations"), nullIfEmpty(item.observations()));
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool insertImages(QSqlDatabase &db, int folio, const QList<domain::VehicleImage> &images,
                  QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_images (vehicle_folio, file_path, is_primary) "
        "VALUES (:vehicle_folio, :file_path, :is_primary)"));

    for (const domain::VehicleImage &image : images) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":file_path"), image.path);
        query.bindValue(QStringLiteral(":is_primary"), image.isPrimary);
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

// upsert: al corregir se borran antes solo los documentos con archivo (los
// que el expediente captura), así que puede quedar un renglón del mismo tipo
// sin archivo; ese se completa en vez de chocar con el UNIQUE.
bool insertDocuments(QSqlDatabase &db, int folio, const QList<domain::VehicleDocument> &documents,
                     QString &errorMessage, bool upsert = false)
{
    QString sql = QStringLiteral(
        "INSERT INTO vehicle_documents (vehicle_folio, document_type, file_path, document_number) "
        "VALUES (:vehicle_folio, :document_type, :file_path, :document_number)");
    if (upsert) {
        sql += QStringLiteral(" ON CONFLICT (vehicle_folio, document_type) DO UPDATE "
                              "   SET file_path = EXCLUDED.file_path, "
                              "       document_number = EXCLUDED.document_number");
    }
    QSqlQuery query(db);
    query.prepare(sql);

    for (const domain::VehicleDocument &document : documents) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":document_type"), document.documentType);
        query.bindValue(QStringLiteral(":file_path"), nullIfEmpty(document.path));
        query.bindValue(QStringLiteral(":document_number"), nullIfEmpty(document.documentNumber));
        if (!query.exec()) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

SqlVehicleRepository::SqlVehicleRepository(ConnectionPool &pool)
    : m_pool(pool)
{
}

bool SqlVehicleRepository::serialNumberExists(const QString &serialNumber, QString *error)
{
    // Los folios son SERIAL, nunca negativos: ninguna unidad queda fuera.
    return serialNumberExistsElsewhere(serialNumber, -1, error);
}

bool SqlVehicleRepository::serialNumberExistsElsewhere(const QString &serialNumber, int folio,
                                                       QString *error)
{
    QString failure;
    bool exists = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT 1 FROM vehicles WHERE serial_number = :vin AND folio <> :folio LIMIT 1"));
        query.bindValue(QStringLiteral(":vin"), serialNumber);
        query.bindValue(QStringLiteral(":folio"), folio);
        if (!db.isOpen() || !query.exec())
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        else
            exists = query.next();
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    }
    return exists;
}

application::VehicleRepository::SaveOutcome SqlVehicleRepository::add(domain::Vehicle &vehicle)
{
    SaveOutcome result;
    bool connectionBroken = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        result = save(db, vehicle);
        connectionBroken = !result.ok && !db.isOpen();
    }
    // Si la operación falló, la conexión de este hilo puede haber quedado
    // inservible (la base se reinició): se descarta para que el reintento abra
    // una nueva.
    if (!result.ok || connectionBroken)
        m_pool.discardThreadConnection();
    return result;
}

application::VehicleRepository::SaveOutcome SqlVehicleRepository::save(QSqlDatabase &db,
                                                                       domain::Vehicle &vehicle)
{
    SaveOutcome result;

    if (!db.isOpen()) {
        result.errorMessage = QStringLiteral("No se pudo conectar con la base de datos: %1")
                                   .arg(db.lastError().text());
        return result;
    }
    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1")
                                   .arg(db.lastError().text());
        return result;
    }

    QString errorMessage;

    SqlCounterpartyRepository counterparties(db);
    const int counterpartyId = counterparties.findOrCreate(vehicle.counterparty(), errorMessage);
    if (counterpartyId < 0) {
        result.errorMessage = QStringLiteral("Error registrando a %1: %2")
                                   .arg(vehicle.counterpartyRole().toLower(), errorMessage);
        db.rollback();
        return result;
    }
    vehicle.assignCounterpartyId(counterpartyId);

    const int folio = insertVehicleRow(db, vehicle, errorMessage);
    if (folio < 0) {
        // La restricción UNIQUE del VIN tiene nombre propio en el esquema:
        // reconocerla deja que el caso de uso lo reporte en el campo, no como
        // una falla genérica.
        result.duplicateSerialNumber = errorMessage.contains(QStringLiteral("vehicles_serial_number_key"));
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1").arg(errorMessage);
        db.rollback();
        return result;
    }

    // Doble despacho: la unidad decide qué subtabla escribir.
    DealInserter dealInserter(db, folio, counterpartyId);
    vehicle.accept(dealInserter);
    if (!dealInserter.ok()) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1")
                                   .arg(dealInserter.errorMessage());
        db.rollback();
        return result;
    }

    if (!insertConditions(db, folio, vehicle.conditions(), errorMessage)
        || !insertInspection(db, folio, vehicle.inspection(), errorMessage)
        || !insertImages(db, folio, vehicle.images(), errorMessage)
        || !insertDocuments(db, folio, vehicle.documents(), errorMessage)) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1").arg(errorMessage);
        db.rollback();
        return result;
    }

    if (!db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1")
                                   .arg(db.lastError().text());
        db.rollback();
        return result;
    }

    vehicle.assignFolio(folio);
    result.ok = true;
    result.folio = folio;
    return result;
}

application::VehicleRepository::UpdateOutcome SqlVehicleRepository::update(
    domain::Vehicle &vehicle, const Expected &expected)
{
    UpdateOutcome result;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        result = rewrite(db, vehicle, expected);
    }
    // Como en add(): con la conexión ya soltada, una que falló se descarta
    // para que el reintento abra otra.
    if (!result.ok)
        m_pool.discardThreadConnection();
    return result;
}

application::VehicleRepository::UpdateOutcome SqlVehicleRepository::rewrite(
    QSqlDatabase &db, domain::Vehicle &vehicle, const Expected &expected)
{
    UpdateOutcome result;

    if (!db.isOpen()) {
        result.errorMessage = QStringLiteral("No se pudo conectar con la base de datos: %1")
                                   .arg(db.lastError().text());
        return result;
    }
    if (!db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1")
                                   .arg(db.lastError().text());
        return result;
    }

    const int folio = vehicle.folio();
    QString errorMessage;
    const auto fail = [&result, &db](const QString &message) {
        result.errorMessage = QStringLiteral("Error corrigiendo el vehículo: %1").arg(message);
        db.rollback();
        return result;
    };

    // 1. La unidad, solo si nadie la cambió desde que se leyó.
    int rowsAffected = 0;
    if (!updateVehicleRow(db, vehicle, expected.vehicleVersion, rowsAffected, errorMessage)) {
        result.duplicateSerialNumber = errorMessage.contains(QStringLiteral("vehicles_serial_number_key"));
        return fail(errorMessage);
    }
    if (rowsAffected != 1) {
        // Nada que actualizar: o ya no existe, o su versión cambió.
        QSqlQuery exists(db);
        exists.prepare(QStringLiteral("SELECT 1 FROM vehicles WHERE folio = :folio"));
        exists.bindValue(QStringLiteral(":folio"), folio);
        if (!exists.exec())
            return fail(exists.lastError().text());
        result.notFound = !exists.next();
        result.conflict = !result.notFound;
        db.rollback();
        return result;
    }

    // 2. Su contraparte. Si el usuario no la cambió, la unidad se queda con la
    // que tiene y esa no se escribe: otro usuario pudo corregirla mientras
    // tanto desde otra operación de la misma persona, y reescribirla con lo
    // que este formulario leyó desharía esa corrección. Si la cambió, la misma
    // persona corregida (solo si sigue como se leyó) u otra.
    int counterpartyId = currentCounterpartyId(db, folio, errorMessage);
    if (counterpartyId < 0)
        return fail(errorMessage);
    if (expected.counterpartyEdited) {
        SqlCounterpartyRepository counterparties(db);
        const SqlCounterpartyRepository::Relink relink = counterparties.keepOrRelink(
            counterpartyId, vehicle.counterparty(), expected.counterpartyVersion);
        if (relink.conflict) {
            result.conflict = true;
            db.rollback();
            return result;
        }
        if (relink.id < 0) {
            result.takenNationalIdOwner = relink.takenBy;
            return fail(relink.takenBy.isEmpty()
                            ? relink.errorMessage
                            : QStringLiteral("la identificación ya es de otra persona"));
        }
        counterpartyId = relink.id;
    }
    vehicle.assignCounterpartyId(counterpartyId);

    // 3. La operación de su rama (doble despacho, como al registrar).
    DealUpdater dealUpdater(db, folio, counterpartyId);
    vehicle.accept(dealUpdater);
    if (!dealUpdater.ok())
        return fail(dealUpdater.errorMessage());

    // 4. Especificaciones, y lo que es una lista se reemplaza completo. De
    // los documentos, solo los que tienen archivo: es el mismo filtro con que
    // los lee el expediente.
    if (!insertConditions(db, folio, vehicle.conditions(), errorMessage, true)
        || !execForFolio(db, QStringLiteral("DELETE FROM vehicle_inspection WHERE vehicle_folio = :folio"),
                         folio, errorMessage)
        || !insertInspection(db, folio, vehicle.inspection(), errorMessage)
        || !execForFolio(db, QStringLiteral("DELETE FROM vehicle_images WHERE vehicle_folio = :folio"),
                         folio, errorMessage)
        || !insertImages(db, folio, vehicle.images(), errorMessage)
        || !execForFolio(db,
                         QStringLiteral("DELETE FROM vehicle_documents "
                                        "WHERE vehicle_folio = :folio AND file_path IS NOT NULL"),
                         folio, errorMessage)
        || !insertDocuments(db, folio, vehicle.documents(), errorMessage, true)) {
        return fail(errorMessage);
    }

    if (!db.commit()) {
        result.commitFailed = true;
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1")
                                   .arg(db.lastError().text());
        db.rollback();
        return result;
    }

    result.ok = true;
    return result;
}
