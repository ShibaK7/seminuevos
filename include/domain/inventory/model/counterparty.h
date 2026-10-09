#ifndef DOMAIN_INVENTORY_MODEL_COUNTERPARTY_H
#define DOMAIN_INVENTORY_MODEL_COUNTERPARTY_H

#include "domain/common/value_objects/validationresult.h"

#include <QString>

namespace domain {

// Persona con la que la agencia hace un trato. Mapea la tabla counterparties.
//
// Una sola clase para todos los papeles -- vendedor, propietario en
// consignación, comprador, aval -- y NO una jerarquía Seller/Buyer/Aval.
// El papel es contextual, no un tipo: la misma persona que hoy vende un auto
// mañana puede comprar otro, y con una clase por rol haría falta un objeto
// distinto por cada sombrero que se ponga la misma persona. Quién es quién en
// una operación lo dice la llave foránea desde la que se le referencia
// (vehicle_acquisitions.seller_id, vehicle_consignments.owner_id), no el tipo
// del objeto.
//
// Los setters con invariante devuelven bool y dejan el objeto intacto cuando
// rechazan: son la red para los llamadores que no son la interfaz (seeders,
// importaciones, lecturas de datos viejos). Los que solo recortan espacios no
// pueden fallar y devuelven void.
class Counterparty
{
public:
    Counterparty() = default;

    // -1 mientras no exista en la base. Lo asigna SqlCounterpartyRepository.
    int id() const;
    bool isPersisted() const;
    void assignId(int id);

    const QString &fullName() const;
    [[nodiscard]] bool setFullName(const QString &value);

    // CURP, INE o RFC. Obligatoria (ver validate()); con ella
    // SqlCounterpartyRepository reconoce a la misma persona en otra operación.
    const QString &nationalId() const;
    [[nodiscard]] bool setNationalId(const QString &value);

    const QString &streetAddress() const;
    void setStreetAddress(const QString &value);

    const QString &suburb() const;
    [[nodiscard]] bool setSuburb(const QString &value);

    const QString &locality() const;
    [[nodiscard]] bool setLocality(const QString &value);

    const QString &state() const;
    [[nodiscard]] bool setState(const QString &value);

    // Vacío, o exactamente 5 dígitos. Se guarda como texto y nunca como
    // entero: hay códigos postales que empiezan con cero (01000, en la Ciudad
    // de México) y un entero se los come.
    const QString &postalCode() const;
    [[nodiscard]] bool setPostalCode(const QString &value);

    // Vacío, o 10 dígitos. Acepta la entrada con espacios, guiones o
    // paréntesis y guarda solo los dígitos, para que dos capturas del mismo
    // número con distinto formato se reconozcan como la misma persona.
    const QString &phone() const;
    [[nodiscard]] bool setPhone(const QString &value);

    const QString &email() const;
    [[nodiscard]] bool setEmail(const QString &value);

    // "calle, colonia, localidad, estado, C.P. 12345", omitiendo las partes
    // vacías. Vive aquí y no en el generador de contratos porque es la forma
    // en que esta entidad se presenta, y la necesita cualquiera que la
    // muestre, no solo el PDF.
    QString formattedAddress() const;

    // El contrato de compraventa exige domicilio del vendedor.
    bool hasCompleteAddress() const;

    // Obligatorios: el nombre y la identificación (la pantalla los marca con
    // asterisco). La identificación además es lo que permite reconocer a la
    // misma persona en otra operación. El resto son datos que la agencia
    // completa cuando los tiene.
    ValidationResult validate() const;

private:
    int m_id = -1;
    QString m_fullName;
    QString m_nationalId;
    QString m_streetAddress;
    QString m_suburb;
    QString m_locality;
    QString m_state;
    QString m_postalCode;
    QString m_phone;
    QString m_email;
};

} // namespace domain

#endif // DOMAIN_INVENTORY_MODEL_COUNTERPARTY_H
