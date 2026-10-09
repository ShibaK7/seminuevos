#ifndef ADAPTERS_CONTRACT_NUMBERTOWORDSES_H
#define ADAPTERS_CONTRACT_NUMBERTOWORDSES_H

#include <QString>

// Convierte un monto en pesos a su representación en letras usada en el
// contrato de compra-venta (p.ej. 204000.00 ->
// "DOSCIENTOS CUATRO MIL PESOS 00/100 M.N."), tal como aparece en la
// cláusula PRIMERA de la plantilla legal.
namespace NumberToWordsEs
{
QString convert(double amount);
} // namespace NumberToWordsEs

#endif // ADAPTERS_CONTRACT_NUMBERTOWORDSES_H
