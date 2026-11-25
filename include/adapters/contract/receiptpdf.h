#ifndef ADAPTERS_CONTRACT_RECEIPTPDF_H
#define ADAPTERS_CONTRACT_RECEIPTPDF_H

#include <QString>

// Lo que comparten los recibos en PDF (mantenimiento y gasto): leer la
// plantilla HTML del recurso, registrar las imágenes que nombra con su ruta
// relativa y dibujarla a PDF con la misma paginación que en un navegador.
// Cada generador solo sustituye sus marcas.
namespace receiptpdf {

// La plantilla en `resourcePath` (":/templates/x.html"), leída como UTF-8.
// Vacía si no se pudo abrir.
QString loadTemplate(const QString &resourcePath);

// Dibuja `html` (ya sin marcas) como PDF en `outputPath`. Devuelve false,
// con la causa en `error`, si falta una imagen de la plantilla.
bool print(const QString &html, const QString &outputPath, QString *error);

} // namespace receiptpdf

#endif // ADAPTERS_CONTRACT_RECEIPTPDF_H
