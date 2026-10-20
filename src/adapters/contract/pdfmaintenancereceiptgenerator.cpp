#include "adapters/contract/pdfmaintenancereceiptgenerator.h"

#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QLocale>
#include <QPageLayout>
#include <QPrinter>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>

namespace {

const QString kTemplatePath = QStringLiteral(":/templates/receipt_maintenance.html");

QString loadTemplate()
{
    QFile file(kTemplatePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

// La plantilla nombra sus imágenes con la ruta relativa que tienen en disco,
// para verse igual en un navegador. QTextDocument no resuelve rutas relativas
// contra qrc:, así que cada una se registra con el nombre exacto que usa la
// plantilla, apuntando a donde la empaqueta Resources.qrc.
bool addImages(QTextDocument &document, QString *missing)
{
    for (const QString &name : {QStringLiteral("logo-seminuevos.jpeg"),
                                QStringLiteral("logo-seminuevos-watermark.png")}) {
        const QImage image(QStringLiteral(":/resources/images/images/") + name);
        if (image.isNull()) {
            *missing = name;
            return false;
        }
        document.addResource(QTextDocument::ImageResource,
                             QUrl(QStringLiteral("../images/") + name), image);
    }
    return true;
}

} // namespace

QString PdfMaintenanceReceiptGenerator::fillTemplate(
    const application::MaintenanceItemDto &maintenance, QString *error)
{
    QString html = loadTemplate();
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
    html.replace(QStringLiteral("[Tipo_Mantenimiento]"), maintenance.typeName.toHtmlEscaped());
    html.replace(QStringLiteral("[Descripción]"),
                 maintenance.description.toHtmlEscaped().replace(QLatin1Char('\n'),
                                                                 QStringLiteral("<br/>")));

    // Como en el contrato: un recibo con una marca cruda parece válido hasta
    // que alguien lo lee con cuidado. Es preferible no generarlo y decir cuál
    // falta.
    static const QRegularExpression leftover(
        QStringLiteral("\\[(Fecha|Costo|Tipo_Mantenimiento|Descripción)\\]"));
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

    QTextDocument document;
    QString missing;
    if (!addImages(document, &missing)) {
        result.errorMessage =
            QStringLiteral("No se encontró la imagen %1 del recibo.").arg(missing);
        return result;
    }
    document.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPath);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    document.print(&printer);

    result.ok = true;
    return result;
}
