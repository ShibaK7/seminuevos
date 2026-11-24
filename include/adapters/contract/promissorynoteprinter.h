#ifndef ADAPTERS_CONTRACT_PROMISSORYNOTEPRINTER_H
#define ADAPTERS_CONTRACT_PROMISSORYNOTEPRINTER_H

#include "application/commercial/sales/dto/saledtos.h"

#include <QString>

// Builds a PDF containing one pagaré page per projected installment.
class PromissoryNotePrinter final
{
public:
    struct Outcome
    {
        bool ok = false;
        QString errorMessage;
    };

    Outcome generatePdf(const application::SaleRegistrationDto &sale,
                        const QString &outputPath) const;

    // Exposed to allow deterministic testing of the generated document without
    // opening a platform print dialog.
    static QString renderHtml(const application::SaleRegistrationDto &sale,
                              QString *errorMessage = nullptr);
};

#endif // ADAPTERS_CONTRACT_PROMISSORYNOTEPRINTER_H
