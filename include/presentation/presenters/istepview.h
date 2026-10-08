#ifndef PRESENTATION_PRESENTERS_ISTEPVIEW_H
#define PRESENTATION_PRESENTERS_ISTEPVIEW_H

#include "domain/value_objects/validationresult.h"

#include <QList>
#include <QString>

namespace presentation {

// Lo que todo paso del asistente sabe hacer con los errores de validación. El
// presenter decide cuáles le tocan a cada paso y cuándo mostrarlos; la vista
// solo sabe dónde está cada campo.
class IStepView
{
public:
    virtual ~IStepView() = default;

    // Marca los campos con error y limpia los demás. Lista vacía = limpiar.
    virtual void showFieldErrors(const QList<domain::ValidationError> &errors) = 0;

    // Le da el foco al campo con esa clave. Devuelve false si no hay un widget
    // visible para ella (la UMA, por ejemplo, no se captura en pantalla).
    virtual bool focusField(const QString &field) = 0;

protected:
    IStepView() = default;
    IStepView(const IStepView &) = default;
    IStepView &operator=(const IStepView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_ISTEPVIEW_H
