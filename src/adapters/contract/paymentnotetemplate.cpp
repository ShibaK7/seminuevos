#include "adapters/contract/paymentnotetemplate.h"

#include "adapters/contract/companyprofile.h"
#include "adapters/contract/numbertowordses.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QIODevice>
#include <QLocale>
#include <QMap>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextDocument>
#include <QTextStream>
#include <QtMath>

namespace {

const QString kTemplatePath = QStringLiteral(":/templates/payment_template.html");

QString dateInSpanish(const QDate &date)
{
    if (!date.isValid())
        return QString();
    return QLocale(QLocale::Spanish, QLocale::Mexico)
        .toString(date, QStringLiteral("d 'de' MMMM 'de' yyyy"));
}

QString loadTemplate(QString *error)
{
    QFile file(kTemplatePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("No se pudo abrir la plantilla del pagaré (%1).")
                     .arg(kTemplatePath);
        return QString();
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

} // namespace

QString PaymentNoteTemplate::fillTemplate(
    const application::CommercialSaleDetailsDto &sale,
    const application::CommercialPaymentDto &payment, QString *error)
{
    QString html = loadTemplate(error);
    if (html.isEmpty())
        return QString();

    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    const QString amountWords = NumberToWordsEs::convert(payment.expectedAmount);
    const QDate signedDate = sale.saleDate;
    const qint64 cents = qRound64(qAbs(payment.expectedAmount) * 100.0) % 100;
    const QMap<QString, QString> values = {
        {QStringLiteral("numero_pagare"), QString::number(payment.installmentNumber)},
        {QStringLiteral("total_pagares"), QString::number(sale.expectedInstallments)},
        {QStringLiteral("monto_numero"), locale.toString(payment.expectedAmount, 'f', 2)},
        {QStringLiteral("monto_letras"), amountWords.section(QStringLiteral(" PESOS "), 0, 0)},
        {QStringLiteral("monto_centavos"),
         QString::number(cents).rightJustified(2, QLatin1Char('0'))},
        {QStringLiteral("lugar_suscripcion"), sale.buyerPopulation},
        {QStringLiteral("dia"), signedDate.isValid() ? QString::number(signedDate.day()) : QString()},
        {QStringLiteral("mes"), signedDate.isValid()
                                    ? locale.toString(signedDate, QStringLiteral("MMMM"))
                                    : QString()},
        {QStringLiteral("anio"), signedDate.isValid()
                                     ? QString::number(signedDate.year()) : QString()},
        {QStringLiteral("beneficiario"), CompanyProfile::name()},
        {QStringLiteral("lugar_pago"), sale.buyerPopulation},
        {QStringLiteral("fecha_pago"), dateInSpanish(payment.scheduledDate)},
        {QStringLiteral("interes_moratorio"), QString()},
        {QStringLiteral("deudor_nombre"), sale.buyerName},
        {QStringLiteral("deudor_domicilio"), sale.buyerAddress},
        {QStringLiteral("deudor_poblacion"), sale.buyerPopulation},
        {QStringLiteral("aval1_nombre"), sale.guarantorName},
        {QStringLiteral("aval1_domicilio"), sale.guarantorAddress},
        {QStringLiteral("aval1_poblacion"), sale.guarantorPopulation},
        {QStringLiteral("aval1_telefono"), sale.guarantorPhone},
        {QStringLiteral("aval2_nombre"), QString()},
        {QStringLiteral("aval2_domicilio"), QString()},
        {QStringLiteral("aval2_poblacion"), QString()},
        {QStringLiteral("aval2_telefono"), QString()},
    };

    for (auto it = values.cbegin(); it != values.cend(); ++it)
        html.replace(QStringLiteral("{{%1}}").arg(it.key()), it.value().toHtmlEscaped());

    static const QRegularExpression leftover(QStringLiteral("\\{\\{([a-z_]+)\\}\\}"));
    const QRegularExpressionMatch match = leftover.match(html);
    if (match.hasMatch()) {
        *error = QStringLiteral("La plantilla del pagaré quedó con el campo '%1' sin llenar.")
                     .arg(match.captured(1));
        return QString();
    }

    error->clear();
    return html;
}

QString PaymentNoteTemplate::suggestedFileName(
    const application::CommercialPaymentDto &payment)
{
    return QStringLiteral("pagare_%1_%2.pdf")
        .arg(payment.saleFolio)
        .arg(payment.installmentNumber);
}

bool PaymentNoteTemplate::writePdf(const QString &html, const QString &outputPath,
                                   QString *error)
{
    {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(outputPath);
        printer.setPageSize(QPageSize(QPageSize::Letter));
        printer.setPageMargins(QMarginsF(7.62, 7.62, 7.62, 7.62), QPageLayout::Millimeter);
        printer.setColorMode(QPrinter::Color);

        QTextDocument document;
        document.setDefaultFont(QFont(QStringLiteral("Segoe UI"), 9));
        document.setHtml(html);
        document.setDocumentMargin(0);
        document.setUseDesignMetrics(true);
        document.setPageSize(printer.pageRect(QPrinter::Inch).size() * 96.0);
        document.print(&printer);
    }

    const QFileInfo output(outputPath);
    if (!output.exists() || output.size() == 0) {
        *error = QStringLiteral("No se pudo generar el archivo PDF del pagaré.");
        return false;
    }

    error->clear();
    return true;
}
