#ifndef PRESENTATION_INVENTORY_REGISTRATION_ISTEPVIEW_H
#define PRESENTATION_INVENTORY_REGISTRATION_ISTEPVIEW_H

#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace presentation {

// Lo que todo paso del asistente sabe hacer con los errores de validación. El
// presenter decide cuáles le tocan a cada paso y cuándo mostrarlos; la vista
// solo sabe dónde está cada campo.
class IStepView
{
public:
    virtual ~IStepView() = default;

    // Marca los campos con error, con sus mensajes debajo, y limpia los
    // demás. Lista vacía = limpiar. Devuelve los errores que no se ven en
    // ningún campo del paso (la UMA, por ejemplo, no se captura en pantalla),
    // para que el asistente los diga en su aviso.
    virtual QList<domain::ValidationError>
    showFieldErrors(const QList<domain::ValidationError> &errors) = 0;

    // Le da el foco al campo con esa clave. Devuelve false si no hay un widget
    // visible para ella (la UMA, por ejemplo, no se captura en pantalla).
    virtual bool focusField(const QString &field) = 0;

    // Solo lectura (el expediente en consulta, #28): los campos muestran su
    // valor pero no cambian, y los botones que capturan (subir, quitar,
    // agregar, reemplazar...) se ocultan; "Ver" sigue. lockedFields son claves
    // "field" que no se pueden cambiar aunque readOnly sea false (al editar el
    // expediente). Vale también para lo que el paso arme después en código:
    // cada paso recuerda su modo.
    virtual void setReadOnly(bool readOnly, const QStringList &lockedFields) = 0;

protected:
    IStepView() = default;
    IStepView(const IStepView &) = default;
    IStepView &operator=(const IStepView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_ISTEPVIEW_H
