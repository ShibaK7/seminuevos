#ifndef CONTRACTPDFGENERATOR_H
#define CONTRACTPDFGENERATOR_H

#include <QString>

struct VehicleDraft;

// Genera el PDF del contrato de compra-venta a partir de
// resources/templates/contract_acquisition.html (plantilla legal aprobada,
// con placeholders {{token}}) usando QTextDocument + QPrinter. No depende de
// ninguna librería de templating -- solo reemplazo de texto.
namespace ContractPdfGenerator
{
struct Result
{
    bool ok = false;
    QString errorMessage;
};

Result generate(const VehicleDraft &draft, const QString &outputPath);
} // namespace ContractPdfGenerator

#endif // CONTRACTPDFGENERATOR_H
