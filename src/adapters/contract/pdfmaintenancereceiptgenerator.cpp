#include "adapters/contract/pdfmaintenancereceiptgenerator.h"

#include "adapters/contract/numbertowordses.h"
#include "adapters/contract/receiptpdf.h"

#include <QLocale>
#include <QRegularExpression>

namespace {

const QString kTemplatePath = QStringLiteral(":/templates/receipt_maintenance.html");

} // namespace

QString PdfMaintenanceReceiptGenerator::fillTemplate(
    const application::MaintenanceItemDto &maintenance, QString *error)
{
    QString html = receiptpdf::loadTemplate(kTemplatePath);
    if (html.isEmpty()) {
        *error = QStringLiteral("No se pudo abrir la plantilla del recibo (%1).").arg(kTemplatePath);
        return QString();
    }

    // Lo que viene del renglón es texto plano: se escapa al sustituir. La
    // descripción conserva sus saltos de línea.
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    html.replace(QStringLiteral("[Fecha]"),
                 locale.toString(maintenance.date, QStringLiteral("d 'de' MMMM 'de' yyyy"))
                     .toHtmlEscaped());
    // El "$" ya está en la plantilla, antes de la marca.
    html.replace(QStringLiteral("[Costo]"), locale.toString(maintenance.cost, 'f', 2));
    html.replace(QStringLiteral("[Costo_Letra]"), NumberToWordsEs::convert(maintenance.cost));
    html.replace(QStringLiteral("[Tipo_Mantenimiento]"), maintenance.typeName.toHtmlEscaped());
    html.replace(QStringLiteral("[Descripción]"),
                 maintenance.description.toHtmlEscaped().replace(QLatin1Char('\n'),
                                                                 QStringLiteral("<br/>")));

    // Como en el contrato: un recibo con una marca cruda parece válido hasta
    // que alguien lo lee con cuidado. Es preferible no generarlo y decir cuál
    // falta.
    static const QRegularExpression leftover(
        QStringLiteral("\\[(Fecha|Costo|Costo_Letra|Tipo_Mantenimiento|Descripción)\\]"));
    const QRegularExpressionMatch match = leftover.match(html);
    if (match.hasMatch()) {
        *error = QStringLiteral("La plantilla del recibo quedó con el campo '%1' sin llenar, así "
                                "que no se generó el documento.").arg(match.captured(1));
        return QString();
    }
    return html;
}

application::MaintenanceReceiptGenerator::Outcome PdfMaintenanceReceiptGenerator::generate(
    const application::MaintenanceItemDto &maintenance, const QString &outputPath)
{
    Outcome result;
    const QString html = fillTemplate(maintenance, &result.errorMessage);
    if (html.isEmpty())
        return result;
    result.ok = receiptpdf::print(html, outputPath, &result.errorMessage);
    return result;
}
