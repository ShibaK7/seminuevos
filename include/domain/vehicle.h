#ifndef DOMAIN_VEHICLE_H
#define DOMAIN_VEHICLE_H

#include "domain/catalogref.h"
#include "domain/counterparty.h"
#include "domain/enums.h"
#include "domain/inspection.h"
#include "domain/validationresult.h"
#include "domain/vehicleconditions.h"
#include "domain/vehicledocument.h"
#include "domain/vehicleimage.h"

#include <QDate>
#include <QList>
#include <QString>

#include <memory>

namespace domain {

class VehicleVisitor;

// Unidad del inventario. Clase base abstracta de la jerarquía que espeja el
// Class Table Inheritance del esquema: la tabla vehicles es el tronco y
// vehicle_acquisitions / vehicle_consignments son las dos ramas.
//
// Una desviación deliberada respecto del esquema: los campos que las DOS
// subtablas comparten (contraparte, factura, mantenimiento, observaciones,
// fecha del trato) viven aquí y no duplicados en cada derivada. Repartirlos
// mecánicamente tabla por tabla habría duplicado seis miembros con sus
// setters sin ganar nada; el reparto por tabla lo hace el repositorio al
// guardar, que es su trabajo. Por eso esta clase NO es un mapa uno a uno de
// la tabla vehicles.
//
// Lo que de verdad difiere entre ramas es cómo se forma el dinero, y eso es
// justo lo que queda virtual.
class Vehicle
{
public:
    virtual ~Vehicle() = default;

    // --- Comportamiento que cambia según la rama -------------------------

    // Discriminador del Class Table Inheritance. Decide en qué subtabla vive
    // el vehículo y qué valor lleva vehicles.acquisition_type.
    virtual AcquisitionType acquisitionType() const = 0;

    // En una adquisición es un precio capturado; en una consignación se
    // calcula a partir del precio base y la comisión, igual que la columna
    // generada del esquema.
    virtual double salePrice() const = 0;

    // Dinero que la agencia realmente puso. No es el mismo concepto en las
    // dos ramas: en consignación el auto nunca se compró, así que lo único
    // invertido es el mantenimiento.
    virtual double totalCost() const = 0;

    // Utilidad esperada. NO se puede escribir como salePrice() - totalCost()
    // en la base: para una consignación eso daría precio base + comisión -
    // mantenimiento, y el precio base se le entrega íntegro al dueño. Ese
    // error, que sería silencioso, es la razón de que el método sea virtual.
    virtual double expectedProfit() const = 0;

    // Los CHECK de invoice_type de las dos subtablas son conjuntos disjuntos.
    // El setter de la base consulta esto antes de aceptar un valor, así que
    // la validación de un campo común termina siendo polimórfica.
    virtual bool acceptsInvoiceType(InvoiceType type) const = 0;

    // Cómo se le llama a la contraparte en esta rama: quien vende la unidad
    // o quien la deja a consignación.
    virtual QString counterpartyRole() const = 0;

    // Doble despacho. Ver vehiclevisitor.h.
    virtual void accept(VehicleVisitor &visitor) const = 0;

    // Constructor virtual. Existe porque el registro corre en otro hilo y el
    // worker reescribe las rutas de los archivos sobre su propio ejemplar:
    // la vista conserva el suyo con las rutas originales, que es lo que hace
    // posible reintentar el guardado después de un error.
    virtual std::unique_ptr<Vehicle> clone() const = 0;

    // --- Validación (Template Method, deliberadamente NO virtual) --------
    // Comprueba lo común a toda unidad y delega lo propio de cada rama en el
    // gancho protegido. Al no ser virtual, ninguna derivada puede sustituirla
    // y saltarse las reglas comunes sin querer.
    //
    // Cubre datos del vehículo, del trato y de la contraparte. NO recurre a
    // las condiciones ni a la inspección: esas tienen su propio validate() y
    // se revisan cuando toca su paso del wizard. Así el Paso 1 puede
    // validarse sin que salten errores de un paso que el usuario todavía no
    // ha visto.
    ValidationResult validate() const;

    // --- Identidad -------------------------------------------------------
    int folio() const;
    bool isPersisted() const;
    void assignFolio(int folio);   // lo llama el repositorio tras el INSERT

    // --- Datos de la unidad ----------------------------------------------
    VehicleStatus status() const;
    void setStatus(VehicleStatus value);

    const QString &serialNumber() const;
    [[nodiscard]] bool setSerialNumber(const QString &value);

    const QString &model() const;
    [[nodiscard]] bool setModel(const QString &value);

    int yearModel() const;
    [[nodiscard]] bool setYearModel(int value);

    int mileage() const;
    [[nodiscard]] bool setMileage(int value);

    const CatalogRef &vehicleType() const;
    void setVehicleType(const CatalogRef &value);

    const CatalogRef &subtype() const;
    void setSubtype(const CatalogRef &value);

    const CatalogRef &brand() const;
    void setBrand(const CatalogRef &value);

    const QString &color() const;
    [[nodiscard]] bool setColor(const QString &value);

    const QString &motorNumber() const;
    [[nodiscard]] bool setMotorNumber(const QString &value);

    const QString &plates() const;
    [[nodiscard]] bool setPlates(const QString &value);

    const QString &platesHolder() const;
    [[nodiscard]] bool setPlatesHolder(const QString &value);

    const QString &repuve() const;
    [[nodiscard]] bool setRepuve(const QString &value);

    const QString &description() const;
    void setDescription(const QString &value);

    // Fecha en que se cerró la operación.
    QDate dealDate() const;
    [[nodiscard]] bool setDealDate(QDate value);

    // Fecha de alta en inventario. Por omisión sigue a la del trato.
    QDate addedDate() const;
    void setAddedDate(QDate value);

    // --- Contraparte (AGREGACIÓN) ----------------------------------------
    // Se guarda por valor, pero la relación es de agregación y no de
    // composición: counterparties no cuelga de vehicles con ON DELETE
    // CASCADE, y la misma persona aparece en muchas operaciones. Lo que hay
    // aquí es una copia local de una entidad compartida, identificada por su
    // id. Borrar el vehículo no debe borrar a la persona.
    const Counterparty &counterparty() const;
    void setCounterparty(const Counterparty &value);
    void assignCounterpartyId(int id);

    // --- Factura y costos comunes a las dos ramas -------------------------
    InvoiceType invoiceType() const;
    [[nodiscard]] bool setInvoiceType(InvoiceType value);

    const QString &invoiceNumber() const;
    [[nodiscard]] bool setInvoiceNumber(const QString &value);

    const QString &invoiceIssuer() const;
    [[nodiscard]] bool setInvoiceIssuer(const QString &value);

    double maintenanceCost() const;
    [[nodiscard]] bool setMaintenanceCost(double value);

    const QString &observations() const;
    void setObservations(const QString &value);

    // --- COMPOSICIÓN ------------------------------------------------------
    // Estas cuatro sí son composición, y el esquema lo dice: sus tablas
    // cuelgan de vehicles con ON DELETE CASCADE. No pueden existir sin la
    // unidad, y se van con ella.
    const VehicleConditions &conditions() const;
    void setConditions(const VehicleConditions &value);

    const Inspection &inspection() const;
    void setInspection(const Inspection &value);

    const QList<VehicleImage> &images() const;
    // false si ya hay una portada y esta también lo es, o si la ruta viene
    // vacía. La regla de "a lo más una portada" vive aquí porque es de la
    // colección, no de la imagen suelta.
    [[nodiscard]] bool addImage(const VehicleImage &image);
    void clearImages();

    const QList<VehicleDocument> &documents() const;
    // false si ya hay un documento de ese tipo -- espejo del UNIQUE
    // (vehicle_folio, document_type) del esquema, aplicado antes de que
    // PostgreSQL tenga que rechazarlo a media transacción.
    [[nodiscard]] bool addDocument(const VehicleDocument &document);
    void clearDocuments();

    // Reemplazan la ruta de un archivo por la definitiva, ya dentro del
    // almacén. Las usa el worker después de copiar cada archivo a disco;
    // devuelven false si el índice está fuera de rango.
    [[nodiscard]] bool setImageStoredPath(int index, const QString &storedPath);
    [[nodiscard]] bool setDocumentStoredPath(int index, const QString &storedPath);

    // --- Consultas de negocio (iguales en las dos ramas) ------------------
    QString displayTitle() const;    // "Nissan Kicks 2021"
    bool isAvailable() const;
    bool hasFaults() const;

protected:
    Vehicle() = default;

    // Copia y movimiento protegidos y por omisión. Protegidos para que nadie
    // pueda copiar a través de una referencia a la base y rebanar el objeto;
    // por omisión y no borrados porque marcarlos = delete aquí borraría
    // implícitamente la copia de las derivadas, que sí la necesitan para
    // implementar clone().
    Vehicle(const Vehicle &) = default;
    Vehicle &operator=(const Vehicle &) = default;
    Vehicle(Vehicle &&) = default;
    Vehicle &operator=(Vehicle &&) = default;

    // Gancho del Template Method: cada rama agrega aquí sus propias reglas.
    virtual void collectSpecificErrors(ValidationResult &result) const = 0;

private:
    int m_folio = -1;
    VehicleStatus m_status = VehicleStatus::Disponible;

    CatalogRef m_vehicleType;
    CatalogRef m_subtype;
    CatalogRef m_brand;

    QString m_model;
    QString m_color;
    QString m_serialNumber;
    QString m_motorNumber;
    QString m_plates;
    QString m_platesHolder;
    QString m_repuve;
    QString m_description;

    int m_yearModel = 0;
    int m_mileage = 0;

    QDate m_dealDate = QDate::currentDate();
    QDate m_addedDate = QDate::currentDate();

    Counterparty m_counterparty;

    InvoiceType m_invoiceType = InvoiceType::Facturado;
    QString m_invoiceNumber;
    QString m_invoiceIssuer;
    double m_maintenanceCost = 0.0;
    QString m_observations;

    VehicleConditions m_conditions;
    Inspection m_inspection;
    QList<VehicleImage> m_images;
    QList<VehicleDocument> m_documents;
};

} // namespace domain

#endif // DOMAIN_VEHICLE_H
