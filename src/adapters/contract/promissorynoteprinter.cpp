#include "adapters/contract/promissorynoteprinter.h"

#include "adapters/contract/numbertowordses.h"

#include <QFile>
#include <QFont>
#include <QIODevice>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringList>
#include <QTextDocument>
#include <QTextStream>

namespace {

QString loadTemplate(QString *errorMessage)
{
    QFile file(QStringLiteral(":/templates/promissory_note.html"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No se pudo abrir la plantilla del pagaré.");
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

QString debtorAddress(const application::SaleBuyerDto &person)
{
    QStringList parts;
    for (const QString &value : {person.streetAddress, person.suburb}) {
        if (!value.trimmed().isEmpty())
            parts << value.trimmed();
    }
    return parts.join(QStringLiteral(", "));
}

QString population(const application::SaleBuyerDto &person)
{
    QStringList parts;
    for (const QString &value : {person.locality, person.state, person.postalCode}) {
        if (!value.trimmed().isEmpty())
            parts << value.trimmed();
    }
    return parts.join(QStringLiteral(", "));
}

QString paymentPage(const QString &sourceTemplate, const application::SaleRegistrationDto &sale,
                    const application::SaleInstallmentDto &installment, int pageIndex)
{
    QString html = sourceTemplate;
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    const int term = sale.financing->monthsTerm;
    const auto replace = [&html](const QString &key, const QString &value) {
        html.replace(QStringLiteral("{{%1}}").arg(key), value.toHtmlEscaped());
    };

    replace(QStringLiteral("installment_number"), QStringLiteral("%1/%2").arg(pageIndex).arg(term));
    replace(QStringLiteral("term"), QString::number(term));
    replace(QStringLiteral("payment_amount"), locale.toCurrencyString(
        installment.expectedAmount, QStringLiteral("$"), 2));
    replace(QStringLiteral("issue_date"), locale.toString(
        sale.saleDate, QStringLiteral("d 'de' MMMM 'de' yyyy")));
    replace(QStringLiteral("creditor"), QStringLiteral("Inmobiliaria y Desarrollo Independencia"));
    replace(QStringLiteral("payment_place"), QStringLiteral("Martínez de la Torre, Ver."));
    replace(QStringLiteral("due_date"), locale.toString(
        installment.dueDate, QStringLiteral("dd/MM/yyyy")));
    replace(QStringLiteral("amount_words"), NumberToWordsEs::convert(installment.expectedAmount));
    replace(QStringLiteral("debtor_name"), sale.buyer.fullName.trimmed());
    replace(QStringLiteral("debtor_address"), debtorAddress(sale.buyer));
    replace(QStringLiteral("debtor_population"), population(sale.buyer));
    replace(QStringLiteral("debtor_phone"), sale.buyer.phone.trimmed());

    application::SaleBuyerDto guarantor;
    if (sale.financing && sale.financing->guarantor)
        guarantor = *sale.financing->guarantor;
    replace(QStringLiteral("guarantor_name"), guarantor.fullName.trimmed());
    replace(QStringLiteral("guarantor_address"), debtorAddress(guarantor));
    replace(QStringLiteral("guarantor_population"), population(guarantor));
    replace(QStringLiteral("guarantor_phone"), guarantor.phone.trimmed());

    static const QRegularExpression leftover(QStringLiteral("\\{\\{([a-z_]+)\\}\\}"));
    const auto match = leftover.match(html);
    if (match.hasMatch())
        return {};
    return html;
}

} // namespace

QString PromissoryNotePrinter::renderHtml(const application::SaleRegistrationDto &sale,
                                          QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (!sale.financing || sale.financing->installments.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Calcula la proyección de pagos antes de imprimir los pagarés.");
        return {};
    }
    if (!sale.saleDate.isValid() || sale.buyer.fullName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Faltan datos de la fecha o del deudor para generar los pagarés.");
        return {};
    }

    QString templateError;
    const QString noteTemplate = loadTemplate(&templateError);
    if (noteTemplate.isEmpty()) {
        if (errorMessage)
            *errorMessage = templateError;
        return {};
    }

    QStringList pages;
    int pageIndex = 1;
    for (const application::SaleInstallmentDto &installment : sale.financing->installments) {
        if (!installment.dueDate.isValid() || installment.expectedAmount < 0.0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("La proyección contiene una fecha o importe inválido.");
            return {};
        }

        const QString page = paymentPage(noteTemplate, sale, installment, pageIndex++);
        if (page.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("No se pudieron completar todos los campos del pagaré.");
            return {};
        }
        pages << page;
    }
    return QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/></head><body>%1</body></html>")
        .arg(pages.join(QStringLiteral("<div style=\"page-break-before: always\"></div>")));
}

PromissoryNotePrinter::Outcome PromissoryNotePrinter::generatePdf(
    const application::SaleRegistrationDto &sale, const QString &outputPath) const
{
    Outcome outcome;
    const QString html = renderHtml(sale, &outcome.errorMessage);
    if (html.isEmpty())
        return outcome;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);

    QTextDocument document;
    document.setDefaultFont(QFont(QStringLiteral("Arial"), 9));
    document.setDocumentMargin(0);
    document.setHtml(html);
    document.setPageSize(printer.pageRect(QPrinter::Point).size());
    document.print(&printer);
    outcome.ok = true;
    return outcome;
}
