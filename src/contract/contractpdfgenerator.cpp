#include "../../include/contract/contractpdfgenerator.h"
#include "../../include/contract/companyprofile.h"
#include "../../include/contract/numbertowordses.h"
#include "../../include/vehiclewizard/vehicledraft.h"

#include <QFile>
#include <QIODevice>
#include <QLocale>
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
        errorMessage = QStringLiteral("No se pudo abrir la plantilla del contrato (:/templates/contract_acquisition.html).");
        return QString();
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

QString buildDocumentsListHtml(const VehicleDraft &draft)
{
    QStringList items;
    if (!draft.invoiceNumber.trimmed().isEmpty()) {
        items << QStringLiteral("<li>FACTURA %1 No. %2 EXPEDIDA POR %3</li>")
                     .arg(draft.invoiceType.toHtmlEscaped(),
                          draft.invoiceNumber.toHtmlEscaped(),
                          draft.invoiceIssuer.toHtmlEscaped());
    }
    for (const PendingDocument &doc : draft.documents)
        items << QStringLiteral("<li>%1</li>").arg(doc.documentType.toHtmlEscaped());

    if (items.isEmpty())
        items << QStringLiteral("<li>Sin documentos registrados.</li>");

    return items.join(QStringLiteral("\n"));
}

QString paymentConditionsText(const VehicleDraft &draft)
{
    if (draft.paymentType.compare(QStringLiteral("Contado"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("PAGO DE CONTADO EN UNA SOLA EXHIBICIÓN.");
    return QStringLiteral("PAGO A CRÉDITO SEGÚN LAS CONDICIONES ACORDADAS ENTRE LAS PARTES.");
}

} // namespace

ContractPdfGenerator::Result ContractPdfGenerator::generate(const VehicleDraft &draft, const QString &outputPath)
{
    Result result;

    QString errorMessage;
    QString html = loadTemplate(errorMessage);
    if (html.isEmpty()) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    const QString priceNumber = locale.toString(draft.purchasePrice, 'f', 2);

    html.replace(QStringLiteral("{{vendedor_nombre}}"), draft.owner.fullName.toHtmlEscaped());
    html.replace(QStringLiteral("{{vendedor_domicilio}}"),
                 QStringLiteral("%1, %2, %3, %4, C.P. %5")
                     .arg(draft.owner.streetAddress, draft.owner.suburb, draft.owner.locality,
                          draft.owner.state, draft.owner.postalCode)
                     .toHtmlEscaped());
    html.replace(QStringLiteral("{{vendedor_identificacion}}"), draft.owner.nationalId.toHtmlEscaped());

    html.replace(QStringLiteral("{{comprador_nombre}}"), CompanyProfile::name().toHtmlEscaped());
    html.replace(QStringLiteral("{{comprador_domicilio}}"), CompanyProfile::address().toHtmlEscaped());
    html.replace(QStringLiteral("{{comprador_identificacion}}"), CompanyProfile::identification().toHtmlEscaped());

    html.replace(QStringLiteral("{{marca}}"), draft.brandName.toHtmlEscaped());
    html.replace(QStringLiteral("{{modelo_anio}}"), QString::number(draft.yearModel));
    html.replace(QStringLiteral("{{tipo}}"), draft.model.toHtmlEscaped());
    html.replace(QStringLiteral("{{no_serie}}"), draft.serialNumber.toHtmlEscaped());
    html.replace(QStringLiteral("{{no_motor}}"), draft.motorNumber.toHtmlEscaped());
    html.replace(QStringLiteral("{{no_placas}}"),
                 draft.plates.trimmed().isEmpty() ? QStringLiteral("SIN PLACA") : draft.plates.toHtmlEscaped());
    html.replace(QStringLiteral("{{color}}"), draft.color.toHtmlEscaped());

    html.replace(QStringLiteral("{{precio_numero}}"), priceNumber);
    html.replace(QStringLiteral("{{precio_letras}}"), NumberToWordsEs::convert(draft.purchasePrice));
    html.replace(QStringLiteral("{{condiciones_pago}}"), paymentConditionsText(draft));

    html.replace(QStringLiteral("{{documentos_lista}}"), buildDocumentsListHtml(draft));
    html.replace(QStringLiteral("{{fecha}}"), draft.date.toString(QStringLiteral("dd/MM/yyyy")));

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
