#ifndef DOMAIN_SETTINGS_VALUE_OBJECTS_SYSTEMPARAMETER_H
#define DOMAIN_SETTINGS_VALUE_OBJECTS_SYSTEMPARAMETER_H

#include <QList>
#include <QString>

#include <optional>

namespace domain {

// Los parámetros globales del negocio que el Administrador fija en
// Configuraciones (US-07.1). Cada uno tiene un solo valor vigente y la fecha
// de su último cambio.
//
// Sus valores se manejan en centésimos (qint64): así cabe igual un monto en
// pesos, en centavos, que un número de UMA, y comparar si algo cambió es
// exacto, sin los redondeos de un double.
enum class SystemParameter {
    UmaDailyValue,        // Valor 1 UMA, en pesos
    CashLimitUmaMultiple, // Máximo valor de pagos en efectivo, en UMA
    BankBalance,          // Cantidad Banco, en pesos
    PettyCashBalance,     // Caja Chica, en pesos
    SafeBalance,          // Caja Fuerte, en pesos
};

enum class ParameterUnit { Pesos, Uma };

// En el orden en que se muestran.
QList<SystemParameter> allSystemParameters();

QString displayLabel(SystemParameter parameter);
ParameterUnit unitOf(SystemParameter parameter);

// El valor que se usa mientras nadie lo ha capturado, en centésimos; nullopt
// si no hay uno razonable (la UMA cambia cada año: no se puede suponer).
std::optional<qint64> defaultHundredths(SystemParameter parameter);

// La clave con que la pantalla marca su campo y el dominio reporta su error.
QString fieldName(SystemParameter parameter);

} // namespace domain

#endif // DOMAIN_SETTINGS_VALUE_OBJECTS_SYSTEMPARAMETER_H
