#ifndef ADAPTERS_CONTRACT_CONTRACTPDFGENERATOR_H
#define ADAPTERS_CONTRACT_CONTRACTPDFGENERATOR_H

#include <QString>

namespace domain {
class Vehicle;
}

// Genera el PDF del contrato a partir de la plantilla HTML empaquetada como
// recurso, sustituyendo las marcas {{...}} por lo que declara la unidad.
//
// El generador no sabe de tipos de operación: pide a la unidad sus valores y
// los sustituye. Quién tiene contrato y qué dice se decide en el dominio, con
// Vehicle::canGenerateContract() y contractPlaceholders().
namespace ContractPdfGenerator
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result generate(const domain::Vehicle &vehicle, const QString &outputPath);
} // namespace ContractPdfGenerator

#endif // ADAPTERS_CONTRACT_CONTRACTPDFGENERATOR_H
