#include "adapters/contract/pdfcontractgenerator.h"
#include "adapters/contract/companyprofile.h"
#include "adapters/contract/numbertowordses.h"

#include <QFile>
#include <QIODevice>
#include <QLocale>
#include <QMap>
#include <QPageLayout>
#include <QPrinter>
#include <QRegularExpression>
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
// plano, así que cada renglón se escapa antes de envolverlo en las etiquetas.
QString buildDocumentsListHtml(const QStringList &lines)
{
    QStringList items;
    for (const QString &line : lines)
        items << QStringLiteral("<li>%1</li>").arg(line.toHtmlEscaped());
    if (items.isEmpty())
        items << QStringLiteral("<li>Sin documentos registrados.</li>");
    return items.join(QStringLiteral("\n"));
}

} // namespace

application::ContractGenerator::Outcome PdfContractGenerator::generate(
    const domain::ContractData &contract, const QString &outputPath)
{
    Outcome result;

    QString errorMessage;
    QString html = loadTemplate(errorMessage);
    if (html.isEmpty()) {
        result.errorMessage = errorMessage;
        return result;
    }

    // Lo que aporta la unidad. Todo texto plano, así que se escapa al
    // sustituir.
    const QMap<QString, QString> &placeholders = contract.placeholders;
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
    const double amount = contract.amount;
    html.replace(QStringLiteral("{{precio_numero}}"), locale.toString(amount, 'f', 2));
    html.replace(QStringLiteral("{{precio_letras}}"), NumberToWordsEs::convert(amount));

    html.replace(QStringLiteral("{{documentos_lista}}"), buildDocumentsListHtml(contract.documentLines));

    // Si quedó alguna marca sin sustituir, el documento saldría con un
    // "{{algo}}" impreso en medio de una cláusula. Es preferible no generar
    // nada y decir cuál falta: un contrato con una marca cruda es peor que
    // ninguno, porque parece válido hasta que alguien lo lee con cuidado.
    static const QRegularExpression leftover(QStringLiteral("\\{\\{([a-z_]+)\\}\\}"));
    const QRegularExpressionMatch match = leftover.match(html);
    if (match.hasMatch()) {
        result.errorMessage = QStringLiteral(
            "La plantilla del contrato quedó con el campo '%1' sin llenar, así que no se "
            "generó el documento.").arg(match.captured(1));
        return result;
    }

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
