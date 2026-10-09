#include "presentation/inventory/acquisition/invoiceattachment.h"

#include <QLatin1Char>

#include <algorithm>

namespace presentation {

namespace {

// El nombre del archivo sin su carpeta, sin preguntarle nada al disco.
QString fileNameOf(const QString &path)
{
    const qsizetype separator =
        std::max(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    return path.mid(separator + 1);
}

} // namespace

void InvoiceAttachment::setAutofactura(bool autofactura)
{
    m_autofactura = autofactura;

    if (m_autofactura) {
        // Con la solicitud ya generada no se aparta nada: cualquier factura
        // adjunta se subió después de generarla, que es justo el orden que se
        // pide.
        if (!m_cfdiRequestGenerated && !m_path.isEmpty()) {
            m_suspendedPath = m_path;
            m_path.clear();
        }
        return;
    }

    // Fuera de Autofactura, la factura apartada vuelve tal como estaba.
    if (!m_suspendedPath.isEmpty()) {
        m_path = m_suspendedPath;
        m_suspendedPath.clear();
    }
}

void InvoiceAttachment::attach(const QString &path)
{
    if (path.isEmpty())
        return;
    m_path = path;
    // Hoy no puede haber una apartada al elegir archivo (mientras la hay, la
    // subida está bloqueada), pero si eso cambiara, salir de Autofactura no
    // debe reemplazar el archivo nuevo por el viejo.
    m_suspendedPath.clear();
}

void InvoiceAttachment::markCfdiRequestGenerated()
{
    m_cfdiRequestGenerated = true;
    // La apartada se elige antes de la solicitud, así que ya no cuenta. Una
    // subida después de una primera solicitud sí vale y se queda.
    m_suspendedPath.clear();
}

QString InvoiceAttachment::attachedPath() const
{
    return m_path;
}

bool InvoiceAttachment::hasSuspendedFile() const
{
    return !m_suspendedPath.isEmpty();
}

bool InvoiceAttachment::cfdiRequestGenerated() const
{
    return m_cfdiRequestGenerated;
}

bool InvoiceAttachment::canUpload() const
{
    return !m_autofactura || m_cfdiRequestGenerated;
}

bool InvoiceAttachment::showsCfdiButton() const
{
    return m_autofactura;
}

QString InvoiceAttachment::label() const
{
    // Apartada no es lo mismo que sin archivo: la etiqueta lo dice para que
    // nadie guarde creyendo que la factura sigue ahí. Es corta a propósito:
    // comparte columna con dos botones.
    if (hasSuspendedFile())
        return QStringLiteral("Factura retirada");
    if (!m_path.isEmpty())
        return fileNameOf(m_path);
    return QStringLiteral("Sin archivo");
}

QString InvoiceAttachment::labelToolTip() const
{
    if (!hasSuspendedFile())
        return QString();
    return QStringLiteral("La autofactura pide generar primero la solicitud de CFDI y subir la "
                          "factura después. Si regresas a Facturado, se recupera la que habías "
                          "elegido.");
}

QString InvoiceAttachment::uploadToolTip() const
{
    // Qt muestra el tooltip aun con el botón deshabilitado, así que ahí se
    // explica el bloqueo.
    return canUpload() ? QString() : QStringLiteral("Primero genera la solicitud de CFDI.");
}

} // namespace presentation
