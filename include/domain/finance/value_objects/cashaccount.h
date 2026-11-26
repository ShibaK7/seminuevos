#ifndef DOMAIN_FINANCE_VALUE_OBJECTS_CASHACCOUNT_H
#define DOMAIN_FINANCE_VALUE_OBJECTS_CASHACCOUNT_H

#include "domain/common/value_objects/enums.h"

#include <QList>
#include <QString>

#include <optional>

namespace domain {

// Dónde está el dinero del negocio. Caja Chica es el efectivo y Cantidad
// Banco lo que entra o sale por transferencia: toda operación de cualquier
// módulo mueve la cuenta de su método de pago. Caja Fuerte solo se llena con
// traspasos desde las otras dos y solo se vacía con retiros.
//
// El orden del enum es el de pantalla: Caja Chica, Cantidad Banco, Caja
// Fuerte.
enum class CashAccount {
    PettyCash, // Caja Chica
    Bank,      // Cantidad Banco
    Safe,      // Caja Fuerte
};

// Las tres, en el orden de pantalla.
QList<CashAccount> allCashAccounts();

QString toDbString(CashAccount account);
std::optional<CashAccount> cashAccountFromDb(const QString &raw);
QString displayLabel(CashAccount account);

// La cuenta que mueve un pago: efectivo → Caja Chica, transferencia →
// Cantidad Banco.
CashAccount accountFor(PaymentMethod method);

} // namespace domain

#endif // DOMAIN_FINANCE_VALUE_OBJECTS_CASHACCOUNT_H
