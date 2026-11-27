#ifndef PRESENTATION_SETTINGS_ISYSTEMSETTINGSVIEW_H
#define PRESENTATION_SETTINGS_ISYSTEMSETTINGSVIEW_H

#include "application/settings/dto/settingsdtos.h"
#include "domain/common/value_objects/validationresult.h"
#include "domain/settings/value_objects/systemparameter.h"
#include "presentation/common/navigation/editsession.h"

#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace presentation {

// Los parámetros de Configuraciones (US-07.1, #82), vistos desde su
// presenter. La implementa SystemSettingsWidget.
class ISystemSettingsView
{
public:
    using EditedValue = std::pair<domain::SystemParameter, std::optional<qint64>>;

    virtual ~ISystemSettingsView() = default;

    // Mientras se lee o se guarda: los botones no responden.
    virtual void setBusy(bool busy) = 0;
    // Pinta cada valor y su "Actualizado el ..."; el que nunca se capturó
    // muestra su valor por omisión y "Sin configurar".
    virtual void showParameters(const QList<application::SystemParameterDto> &parameters) = 0;
    // Lo que hay en cada campo, en centésimos; nullopt si está vacío.
    virtual QList<EditedValue> editedValues() const = 0;

    // En View los campos son de solo lectura y se ofrece "Editar" (si
    // canEdit); en Edit se pueden cambiar y se ofrecen "Guardar" y
    // "Cancelar".
    virtual void showMode(FormMode mode, bool canEdit) = 0;
    virtual void showFieldErrors(const QList<domain::ValidationError> &errors) = 0;
    // Un error que no es de un campo (la base, un conflicto).
    virtual void showMessage(const QString &message) = 0;
    // Un aviso que no es un error ("Cambios guardados").
    virtual void showNotice(const QString &notice) = 0;
    // "¿Descartar los cambios?". true si acepta perderlos.
    virtual bool confirmDiscardChanges() = 0;

protected:
    ISystemSettingsView() = default;
    ISystemSettingsView(const ISystemSettingsView &) = default;
    ISystemSettingsView &operator=(const ISystemSettingsView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_SETTINGS_ISYSTEMSETTINGSVIEW_H
