#include "adapters/contract/pdfexpensereceiptgenerator.h"

#include "adapters/contract/numbertowordses.h"
#include "adapters/contract/receiptpdf.h"

#include <QLocale>
#include <QRegularExpression>

namespace {

const QString kTemplatePath = QStringLiteral(":/templates/receipt_expense.html");

} // namespace

QString PdfExpenseReceiptGenerator::fillTemplate(
    const application::OperationalExpenseItemDto &expense, QString *error)
{
    QString html = receiptpdf::loadTemplate(kTemplatePath);
    if (html.isEmpty()) {
        *error = QStringLiteral("No se pudo abrir la plantilla del recibo (%1).").arg(kTemplatePath);
        return QString();
    }

    // Lo que viene del renglón es texto plano: se escapa al sustituir. La
    // descripción conserva sus saltos de línea. Los importes van sin "$": la
    // plantilla lo pone antes de la marca.
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    const auto money = [&locale](double amount) { return locale.toString(amount, 'f', 2); };
    html.replace(QStringLiteral("[Fecha]"),
                 locale.toString(expense.date, QStringLiteral("d 'de' MMMM 'de' yyyy"))
                     .toHtmlEscaped());
    html.replace(QStringLiteral("[Total]"), money(expense.total));
    html.replace(QStringLiteral("[Total_Letra]"), NumberToWordsEs::convert(expense.total));
    html.replace(QStringLiteral("[Tipo_Gasto]"), expense.typeName.toHtmlEscaped());
    html.replace(QStringLiteral("[Descripción]"),
                 expense.description.toHtmlEscaped().replace(QLatin1Char('\n'),
                                                             QStringLiteral("<br/>")));
    html.replace(QStringLiteral("[Folio]"), QString::number(expense.folio));
    html.replace(QStringLiteral("[Sub_Total]"), money(expense.subtotal));
    html.replace(QStringLiteral("[IVA]"), money(expense.iva));
    html.replace(QStringLiteral("[No_Factura]"),
                 expense.hasInvoice ? expense.invoiceNumber.toHtmlEscaped()
                                    : QStringLiteral("Sin Factura"));
    html.replace(QStringLiteral("[Método_Pago]"),
                 expense.paymentMethod ? domain::displayLabel(*expense.paymentMethod) : QString());

    // Como en el recibo de mantenimiento: un recibo con una marca cruda
    // parece válido hasta que alguien lo lee con cuidado. Es preferible no
    // generarlo y decir cuál falta.
    static const QRegularExpression leftover(
        QStringLiteral("\\[(Fecha|Total|Total_Letra|Tipo_Gasto|Descripción)\\]"));
    const QRegularExpressionMatch match = leftover.match(html);
    if (match.hasMatch()) {
        *error = QStringLiteral("La plantilla del recibo quedó con el campo '%1' sin llenar, así "
                                "que no se generó el documento.").arg(match.captured(1));
        return QString();
    }
    return html;
}

application::ExpenseReceiptGenerator::Outcome PdfExpenseReceiptGenerator::generate(
    const application::OperationalExpenseItemDto &expense, const QString &outputPath)
{
    Outcome result;
    const QString html = fillTemplate(expense, &result.errorMessage);
    if (html.isEmpty())
        return result;
    result.ok = receiptpdf::print(html, outputPath, &result.errorMessage);
    return result;
}
