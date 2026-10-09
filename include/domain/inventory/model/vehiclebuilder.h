#ifndef DOMAIN_INVENTORY_MODEL_VEHICLEBUILDER_H
#define DOMAIN_INVENTORY_MODEL_VEHICLEBUILDER_H

#include "domain/inventory/model/acquisition/acquiredvehicle.h"
#include "domain/inventory/model/consignment/consignedvehicle.h"
#include "domain/inventory/model/vehicle.h"

#include <QDate>
#include <QString>

#include <memory>

namespace domain {

// Arma un Vehicle a partir de lo que el usuario captura en los tres pasos del
// asistente. Es el único camino sancionado para obtener una unidad válida.
//
// Existe por dos problemas concretos que no se resuelven con un constructor:
//
//   1. La rama (compra o consignación) se decide en el primer paso, así que
//      al abrir el asistente todavía no se sabe qué subclase instanciar.
//   2. Una unidad con invariantes no puede existir a medias, pero la captura
//      necesariamente pasa por estados incompletos.
//
// El builder absorbe ambos: acepta datos parciales, decide la subclase en
// cuanto conoce el tipo, y solo entrega el objeto cuando pasa la validación.
// Ese `build()` que elige entre AcquiredVehicle y ConsignedVehicle es el
// único lugar del programa donde se ramifica por tipo de operación.
//
// Los setters de la unidad devuelven bool cuando rechazan un valor imposible.
// Aquí ese bool no se pierde: el builder lo convierte en un error con el
// nombre del campo, de modo que "kilometraje negativo" llegue a la pantalla
// en vez de descartarse en silencio.
//
// ORDEN: setAcquisitionType() debe llamarse antes que cualquier otro setter.
// Si no se llama, se asume una compra.
class VehicleBuilder
{
public:
    VehicleBuilder();
    ~VehicleBuilder();

    VehicleBuilder(const VehicleBuilder &) = delete;
    VehicleBuilder &operator=(const VehicleBuilder &) = delete;
    VehicleBuilder(VehicleBuilder &&) noexcept;
    VehicleBuilder &operator=(VehicleBuilder &&) noexcept;

    // Decide la subclase. Llamarlo con un tipo distinto reinicia la captura,
    // porque los datos propios de una rama no tienen equivalente en la otra.
    VehicleBuilder &setAcquisitionType(AcquisitionType type);
    AcquisitionType acquisitionType() const;

    // --- Paso 1: datos comunes -------------------------------------------
    VehicleBuilder &setDealDate(QDate value);
    VehicleBuilder &setVehicleType(const CatalogRef &value);
    VehicleBuilder &setSubtype(const CatalogRef &value);
    VehicleBuilder &setBrand(const CatalogRef &value);
    VehicleBuilder &setModel(const QString &value);
    VehicleBuilder &setYearModel(int value);
    VehicleBuilder &setColor(const QString &value);
    VehicleBuilder &setMileage(int value);
    VehicleBuilder &setSerialNumber(const QString &value);
    VehicleBuilder &setMotorNumber(const QString &value);
    VehicleBuilder &setPlates(const QString &value);
    VehicleBuilder &setPlatesHolder(const QString &value);
    VehicleBuilder &setRepuve(const QString &value);
    VehicleBuilder &setDescription(const QString &value);
    VehicleBuilder &setCounterparty(const Counterparty &value);
    VehicleBuilder &setInvoiceType(InvoiceType value);
    VehicleBuilder &setInvoiceNumber(const QString &value);
    VehicleBuilder &setInvoiceIssuer(const QString &value);
    VehicleBuilder &setMaintenanceCost(double value);
    VehicleBuilder &setObservations(const QString &value);

    // --- Paso 1: solo compra ----------------------------------------------
    // Se ignoran en silencio si la rama es consignación: el asistente oculta
    // esos campos, y hacerlos fallar convertiría un cambio de tipo en una
    // cascada de errores sobre datos que ya no se muestran.
    VehicleBuilder &setPurchasePrice(double value);
    VehicleBuilder &setSalePrice(double value);
    VehicleBuilder &setPaymentType(PaymentType value);
    VehicleBuilder &setPaymentMethod(PaymentMethod value);
    VehicleBuilder &setInvoiceFilePath(const QString &value);
    // UMA vigente, leída de la configuración por la capa de aplicación. Sin
    // ella la regla del pago en efectivo no se puede evaluar.
    VehicleBuilder &setUmaDailyValue(double value);

    // --- Paso 1: solo consignación ----------------------------------------
    VehicleBuilder &setBasePrice(double value);
    VehicleBuilder &setCommissionRate(double value);

    // --- Pasos 2 y 3 -------------------------------------------------------
    VehicleBuilder &setConditions(const VehicleConditions &value);
    VehicleBuilder &setInspection(const Inspection &value);
    VehicleBuilder &addImage(const VehicleImage &value);
    VehicleBuilder &addDocument(const VehicleDocument &value);
    VehicleBuilder &clearFiles();

    // --- Validación por paso ----------------------------------------------
    // Reflejan los pasos del asistente: cada uno comprueba lo suyo, para que
    // el primero no reclame datos que se capturan en el segundo.
    ValidationResult validateVehicleData() const;
    ValidationResult validateConditionData() const;

    // Entrega la unidad. Devuelve nullptr si algo no valida, con el detalle
    // en `result`. Después de esto el builder queda vacío: el objeto sale de
    // aquí y el builder ya no lo comparte.
    std::unique_ptr<Vehicle> build(ValidationResult &result);

private:
    void instantiate(AcquisitionType type);
    // Registra el rechazo de un setter, para no perderlo.
    void noteRejected(const QString &field, const QString &message);

    std::unique_ptr<Vehicle> m_vehicle;
    // Alias sin propiedad hacia m_vehicle, para llegar a los datos de cada
    // rama sin dynamic_cast. Solo uno de los dos es distinto de nullptr.
    AcquiredVehicle *m_acquired = nullptr;
    ConsignedVehicle *m_consigned = nullptr;

    AcquisitionType m_type = AcquisitionType::Adquisicion;
    ValidationResult m_rejected;
};

} // namespace domain

#endif // DOMAIN_INVENTORY_MODEL_VEHICLEBUILDER_H
