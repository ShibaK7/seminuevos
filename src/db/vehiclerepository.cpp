#include "../../include/db/vehiclerepository.h"

#include "../../include/db/counterpartyrepository.h"
#include "domain/acquiredvehicle.h"
#include "domain/consignedvehicle.h"
#include "domain/vehicle.h"
#include "domain/vehiclevisitor.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QVariant nullIfEmpty(const QString &value)
{
    if (value.isEmpty())
        return QVariant(QMetaType(QMetaType::QString));
    return value;
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

bool insertConditions(QSqlDatabase &db, int folio, const domain::VehicleConditions &conditions,
                      QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_conditions "
        "(vehicle_folio, fuel_type_id, cylinders, transmission, interior_material, "
        " window_regulators, air_conditioning) "
        "VALUES (:vehicle_folio, :fuel_type_id, :cylinders, :transmission, :interior_material, "
        " :window_regulators, :air_conditioning)"));
    query.bindValue(QStringLiteral(":vehicle_folio"), folio);
    query.bindValue(QStringLiteral(":fuel_type_id"), catalogId(conditions.fuelType()));
    query.bindValue(QStringLiteral(":cylinders"), conditions.cylinders());
    query.bindValue(QStringLiteral(":transmission"), domain::toDbString(conditions.transmission()));
    query.bindValue(QStringLiteral(":interior_material"), nullIfEmpty(conditions.interiorMaterial()));
    query.bindValue(QStringLiteral(":window_regulators"),
                    domain::toDbString(conditions.windowRegulators()));
    query.bindValue(QStringLiteral(":air_conditioning"),
                    domain::toDbString(conditions.airConditioning()));

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
        "(vehicle_folio, element_id, is_checked, is_optimal, observations) "
        "VALUES (:vehicle_folio, :element_id, :is_checked, :is_optimal, :observations)"));

    for (const domain::InspectionItem &item : inspection.items()) {
        query.bindValue(QStringLiteral(":vehicle_folio"), folio);
        query.bindValue(QStringLiteral(":element_id"), item.elementId());
        query.bindValue(QStringLiteral(":is_checked"), item.isChecked());
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

bool insertDocuments(QSqlDatabase &db, int folio, const QList<domain::VehicleDocument> &documents,
                     QString &errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO vehicle_documents (vehicle_folio, document_type, file_path, document_number) "
        "VALUES (:vehicle_folio, :document_type, :file_path, :document_number)"));

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

VehicleRepository::VehicleRepository(QSqlDatabase &db)
    : m_db(db)
{
}

VehicleRepository::Result VehicleRepository::save(domain::Vehicle &vehicle)
{
    Result result;

    if (!m_db.transaction()) {
        result.errorMessage = QStringLiteral("No se pudo iniciar la transacción: %1")
                                   .arg(m_db.lastError().text());
        return result;
    }

    QString errorMessage;

    CounterpartyRepository counterparties(m_db);
    const int counterpartyId = counterparties.findOrCreate(vehicle.counterparty(), errorMessage);
    if (counterpartyId < 0) {
        result.errorMessage = QStringLiteral("Error registrando a %1: %2")
                                   .arg(vehicle.counterpartyRole().toLower(), errorMessage);
        m_db.rollback();
        return result;
    }
    vehicle.assignCounterpartyId(counterpartyId);

    const int folio = insertVehicleRow(m_db, vehicle, errorMessage);
    if (folio < 0) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1").arg(errorMessage);
        m_db.rollback();
        return result;
    }

    // Doble despacho: la unidad decide qué subtabla escribir.
    DealInserter dealInserter(m_db, folio, counterpartyId);
    vehicle.accept(dealInserter);
    if (!dealInserter.ok()) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1")
                                   .arg(dealInserter.errorMessage());
        m_db.rollback();
        return result;
    }

    if (!insertConditions(m_db, folio, vehicle.conditions(), errorMessage)
        || !insertInspection(m_db, folio, vehicle.inspection(), errorMessage)
        || !insertImages(m_db, folio, vehicle.images(), errorMessage)
        || !insertDocuments(m_db, folio, vehicle.documents(), errorMessage)) {
        result.errorMessage = QStringLiteral("Error registrando el vehículo: %1").arg(errorMessage);
        m_db.rollback();
        return result;
    }

    if (!m_db.commit()) {
        result.errorMessage = QStringLiteral("No se pudo confirmar la transacción: %1")
                                   .arg(m_db.lastError().text());
        m_db.rollback();
        return result;
    }

    vehicle.assignFolio(folio);
    result.ok = true;
    result.folio = folio;
    return result;
}
