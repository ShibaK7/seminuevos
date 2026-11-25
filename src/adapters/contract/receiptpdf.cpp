#include "adapters/contract/receiptpdf.h"

#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QPageLayout>
#include <QPrinter>
#include <QStringConverter>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>

namespace receiptpdf {

namespace {

// Las plantillas nombran sus imágenes con la ruta relativa que tienen en
// disco, para verse igual en un navegador. QTextDocument no resuelve rutas
// relativas contra qrc:, así que cada una se registra con el nombre exacto que
// usa la plantilla, apuntando a donde la empaqueta Resources.qrc.
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

QString loadTemplate(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

bool print(const QString &html, const QString &outputPath, QString *error)
{
    QTextDocument document;
    QString missing;
    if (!addImages(document, &missing)) {
        *error = QStringLiteral("No se encontró la imagen %1 del recibo.").arg(missing);
        return false;
    }
    document.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPath);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    // El documento se pagina aquí, a la resolución en que está pensada la
    // plantilla (96 px por pulgada, la de un navegador), en el área imprimible
    // de la hoja. Sin tamaño de página, print() lo vuelve a acomodar a la
    // resolución de la impresora (1200 ppp): agrega su propio margen y un
    // número de página, escala las <img> pero no el fondo de una celda, y la
    // marca de agua sale como un mosaico de logos diminutos. Paginado, print()
    // escala la hoja entera por igual y sale como en el navegador.
    constexpr qreal kTemplateDpi = 96.0;
    const QSizeF printable = printer.pageRect(QPrinter::Inch).size();
    document.setDocumentMargin(0);
    document.setPageSize(printable * kTemplateDpi);

    document.print(&printer);
    return true;
}

} // namespace receiptpdf
