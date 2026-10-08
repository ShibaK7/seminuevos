#ifndef ADAPTERS_CONTRACT_PDFCONTRACTGENERATOR_H
#define ADAPTERS_CONTRACT_PDFCONTRACTGENERATOR_H

#include "application/ports/contractgenerator.h"

// Adaptador del puerto ContractGenerator: genera el PDF del contrato a partir
// de la plantilla HTML empaquetada como recurso, sustituyendo las marcas
// {{...}}.
//
// No sabe de tipos de operación ni de entidades: recibe ContractData, que el
// dominio ya decidió (Vehicle::contractData()), y solo lo dibuja. Compra y
// consignación usan la misma plantilla; solo cambia el título.
class PdfContractGenerator final : public application::ContractGenerator
{
public:
    Outcome generate(const domain::ContractData &contract, const QString &outputPath) override;
};

#endif // ADAPTERS_CONTRACT_PDFCONTRACTGENERATOR_H
