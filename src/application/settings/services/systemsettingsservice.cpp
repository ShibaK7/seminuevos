#include "application/settings/services/systemsettingsservice.h"

#include "domain/settings/rules/settingsaccess.h"
#include "domain/settings/rules/systemparameterpolicy.h"

namespace application {

SystemSettingsService::SystemSettingsService(SettingsRepository &settings)
    : m_settings(settings)
{
}

SystemParametersResult SystemSettingsService::getSystemParameters(domain::UserRole role) const
{
    using Status = SystemParametersResult::Status;
    SystemParametersResult result;
    if (!domain::SettingsAccess(role).canManageSystemParameters()) {
        result.status = Status::Denied;
        return result;
    }

    QString error;
    const QList<SystemParameterDto> stored = m_settings.getSystemParameters(&error);
    if (!error.isEmpty()) {
        result.status = Status::Failed;
        result.errorMessage = error;
        return result;
    }

    // Siempre los cinco y en orden: el que no tiene renglón se muestra "sin
    // configurar", no desaparece de la pantalla.
    for (const domain::SystemParameter parameter : domain::allSystemParameters()) {
        SystemParameterDto dto;
        dto.parameter = parameter;
        for (const SystemParameterDto &row : stored) {
            if (row.parameter == parameter)
                dto = row;
        }
        result.parameters << dto;
    }
    result.status = Status::Loaded;
    return result;
}

SystemParametersUpdateResult SystemSettingsService::updateSystemParameters(
    const QList<SystemParameterEditDto> &edits, domain::UserRole role) const
{
    using Status = SystemParametersUpdateResult::Status;
    SystemParametersUpdateResult result;
    if (!domain::SettingsAccess(role).canManageSystemParameters()) {
        result.status = Status::Denied;
        return result;
    }

    QList<SettingsRepository::Change> changes;
    domain::ValidationResult validation;
    for (const SystemParameterEditDto &edit : edits) {
        // Lo que había: lo leído o, si nunca se capturó, su valor por omisión
        // (el que la pantalla mostró). Si sigue igual, no cambió.
        const std::optional<qint64> before =
            edit.loaded ? edit.loaded : domain::defaultHundredths(edit.parameter);
        if (edit.edited == before)
            continue;
        if (!edit.edited) {
            validation.addError(domain::fieldName(edit.parameter),
                                QStringLiteral("Captura un valor: este parámetro no puede quedar "
                                               "vacío."));
            continue;
        }
        domain::SystemParameterPolicy::check(edit.parameter, *edit.edited, validation);
        changes << SettingsRepository::Change{edit.parameter, edit.loaded, *edit.edited};
    }

    if (!validation.isValid()) {
        result.status = Status::Rejected;
        result.validation = validation;
        return result;
    }
    if (changes.isEmpty()) {
        result.status = Status::NoChanges;
        return result;
    }

    const SettingsRepository::UpdateOutcome outcome = m_settings.updateSystemParameters(changes);
    switch (outcome.status) {
    case SettingsRepository::UpdateOutcome::Status::Updated:
        result.status = Status::Saved;
        for (const SettingsRepository::Change &change : changes)
            result.updated << change.parameter;
        return result;
    case SettingsRepository::UpdateOutcome::Status::Conflict:
        result.status = Status::Conflict;
        return result;
    case SettingsRepository::UpdateOutcome::Status::Failed:
        break;
    }
    result.status = Status::Failed;
    result.errorMessage = outcome.errorMessage;
    return result;
}

} // namespace application
