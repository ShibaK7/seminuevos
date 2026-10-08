#ifndef PRESENTATION_PRESENTERS_INVOICEATTACHMENT_H
#define PRESENTATION_PRESENTERS_INVOICEATTACHMENT_H

#include <QString>

namespace presentation {

// La factura adjunta del Paso 1 y la regla de la autofactura: en ella, la
// factura se sube DESPUÉS de generar la solicitud de CFDI.
//
// Bloquear el botón no alcanza para garantizarlo: con Facturado, el tipo por
// omisión, la subida está libre, y una factura elegida ahí seguiría adjunta al
// pasar a Autofactura sin que la solicitud se hubiera generado. Por eso:
//   - Pasar a Autofactura sin solicitud APARTA la factura: deja de contar como
//     adjunta, pero se conserva.
//   - Salir de Autofactura la recupera. Ir y volver en el combo es fácil de
//     hacer sin querer, y no debe costar en silencio la factura elegida.
//   - Generar la solicitud la descarta: en autofactura solo vale la que se
//     sube después. Y queda generada para siempre: ir y volver entre tipos no
//     obliga a generarla otra vez.
//
// Antes estas reglas vivían repartidas en cuatro métodos de la vista, que
// tocaban botones y etiquetas cada uno por su cuenta. Aquí son estado puro, y
// la vista pinta lo que esta clase dice.
class InvoiceAttachment
{
public:
    // Cambió el tipo de factura. Aparta o recupera según la regla de arriba.
    void setAutofactura(bool autofactura);
    // El usuario eligió un archivo. Deja sin efecto cualquier factura apartada.
    void attach(const QString &path);
    // La solicitud de CFDI se abrió bien.
    void markCfdiRequestGenerated();

    // La factura que cuenta como adjunta; vacía si no hay o si está apartada.
    QString attachedPath() const;
    bool hasSuspendedFile() const;
    bool cfdiRequestGenerated() const;

    bool canUpload() const;
    bool showsCfdiButton() const;
    QString label() const;
    QString labelToolTip() const;
    QString uploadToolTip() const;

private:
    bool m_autofactura = false;
    bool m_cfdiRequestGenerated = false;
    QString m_path;
    QString m_suspendedPath;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_INVOICEATTACHMENT_H
