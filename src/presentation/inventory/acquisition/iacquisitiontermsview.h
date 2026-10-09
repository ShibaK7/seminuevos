#ifndef PRESENTATION_INVENTORY_ACQUISITION_IACQUISITIONTERMSVIEW_H
#define PRESENTATION_INVENTORY_ACQUISITION_IACQUISITIONTERMSVIEW_H

#include <QString>

namespace presentation {

// Cómo se ve la fila de la factura. Lo decide InvoiceAttachment; la vista
// solo lo pinta.
struct InvoiceAttachmentState
{
    QString fileLabel;     // "Sin archivo", el nombre del archivo o "Factura retirada"
    QString fileToolTip;   // por qué se retiró, si se retiró
    bool cfdiButtonVisible = false;
    bool uploadEnabled = true;
    QString uploadToolTip; // por qué está bloqueada la subida
};

// Lo propio de una compra dentro del Paso 1: el archivo de la factura (y la
// solicitud de CFDI de la autofactura). Los precios y el pago de esta sección
// no necesitan nada del presenter: viajan con lo capturado del paso.
class IAcquisitionTermsView
{
public:
    virtual ~IAcquisitionTermsView() = default;

    virtual void showInvoiceAttachment(const InvoiceAttachmentState &state) = 0;

    // Pide al usuario el archivo de la factura. Vacío si canceló.
    virtual QString askInvoiceFile() = 0;
    // Abre un archivo con el programa que el sistema tenga para él.
    virtual bool openDocument(const QString &path) = 0;
    virtual void showWarning(const QString &title, const QString &message) = 0;

protected:
    IAcquisitionTermsView() = default;
    IAcquisitionTermsView(const IAcquisitionTermsView &) = default;
    IAcquisitionTermsView &operator=(const IAcquisitionTermsView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_ACQUISITION_IACQUISITIONTERMSVIEW_H
