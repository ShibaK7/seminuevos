#include "../../include/contract/contractpdfgenerator.h"
#include "../../include/contract/companyprofile.h"
#include "../../include/contract/numbertowordses.h"
#include "domain/vehicle.h"

#include <QFile>
#include <QIODevice>
#include <QLocale>
#include <QMap>
#include <QPageLayout>
#include <QPrinter>
#include <QStringConverter>
#include <QStringList>
#include <QTextDocument>
#include <QTextStream>

namespace {

QString loadTemplate(QString &errorMessage)
{
    QFile file(QStringLiteral(":/templates/contract_acquisition.html"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("No se pudo abrir la plantilla del contrato "
                                       "(:/templates/contract_acquisition.html).");
        return QString();
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

// La lista de documentos es lo único que se arma como HTML y no como texto
// plano, así que cada campo se escapa por separado antes de envolverlo en las
// etiquetas.
QString buildDocumentsListHtml(const domain::Vehicle &vehicle)
{
    QStringList items;

    if (!vehicle.invoiceNumber().trimmed().isEmpty()) {
        items << QStringLiteral("<li>FACTURA %1 No. %2 EXPEDIDA POR %3</li>")
                     .arg(domain::toDbString(vehicle.invoiceType()).toHtmlEscaped(),
                          vehicle.invoiceNumber().toHtmlEscaped(),
                          vehicle.invoiceIssuer().toHtmlEscaped());
    }
    for (const domain::VehicleDocument &document : vehicle.documents())
        items << QStringLiteral("<li>%1</li>").arg(document.documentType.toHtmlEscaped());

    if (items.isEmpty())
        items << QStringLiteral("<li>Sin documentos registrados.</li>");

    return items.join(QStringLiteral("\n"));
}

} // namespace

ContractPdfGenerator::Result ContractPdfGenerator::generate(const domain::Vehicle &vehicle,
                                                            const QString &outputPath)
{
    Result result;

    if (!vehicle.canGenerateContract()) {
        result.errorMessage = QStringLiteral(
            "Esta operación no genera contrato de compraventa. La plantilla disponible "
            "declara que el vendedor recibe el importe del vehículo, cosa que no ocurre "
            "en una consignación.");
        return result;
    }

    QString errorMessage;
    QString html = loadTemplate(errorMessage);
    if (html.isEmpty()) {
        result.errorMessage = errorMessage;
        return result;
    }

    // Lo que aporta la unidad. Todo texto plano, así que se escapa al
    // sustituir.
    const QMap<QString, QString> placeholders = vehicle.contractPlaceholders();
    for (auto it = placeholders.constBegin(); it != placeholders.constEnd(); ++it) {
        html.replace(QStringLiteral("{{%1}}").arg(it.key()), it.value().toHtmlEscaped());
    }

    // Datos de la agencia: son de la aplicación, no de la unidad.
    html.replace(QStringLiteral("{{comprador_nombre}}"), CompanyProfile::name().toHtmlEscaped());
    html.replace(QStringLiteral("{{comprador_domicilio}}"), CompanyProfile::address().toHtmlEscaped());
    html.replace(QStringLiteral("{{comprador_identificacion}}"),
                 CompanyProfile::identification().toHtmlEscaped());

    // El importe llega como número y se formatea aquí: convertirlo a letras o
    // ponerle separadores de miles es presentación, no negocio.
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    const double amount = vehicle.contractAmount();
    html.replace(QStringLiteral("{{precio_numero}}"), locale.toString(amount, 'f', 2));
    html.replace(QStringLiteral("{{precio_letras}}"), NumberToWordsEs::convert(amount));

    html.replace(QStringLiteral("{{documentos_lista}}"), buildDocumentsListHtml(vehicle));

    QTextDocument document;
    document.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPath);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    document.print(&printer);

    result.ok = true;
    return result;
}
