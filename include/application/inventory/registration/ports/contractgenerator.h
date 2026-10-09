#ifndef APPLICATION_INVENTORY_REGISTRATION_PORTS_CONTRACTGENERATOR_H
#define APPLICATION_INVENTORY_REGISTRATION_PORTS_CONTRACTGENERATOR_H

#include "domain/inventory/value_objects/contractdata.h"

#include <QString>

namespace application {

// Puerto: cómo se produce el documento del contrato. Recibe lo que el dominio
// ya decidió (ContractData) y solo lo dibuja; hoy, un PDF con QPrinter.
class ContractGenerator
{
public:
    struct Outcome
    {
        bool ok = false;
        QString errorMessage;
    };

    virtual ~ContractGenerator() = default;

    virtual Outcome generate(const domain::ContractData &contract, const QString &outputPath) = 0;

protected:
    ContractGenerator() = default;
    ContractGenerator(const ContractGenerator &) = default;
    ContractGenerator &operator=(const ContractGenerator &) = default;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_PORTS_CONTRACTGENERATOR_H
