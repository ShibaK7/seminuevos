#ifndef TESTS_SUPPORT_SETTINGSFAKES_H
#define TESTS_SUPPORT_SETTINGSFAKES_H

// Puertos falsos de Configuraciones (US-07.1). Van aparte de fakes.h para que
// las pruebas de los demás módulos no dependan de ellos.

#include "application/settings/ports/settingsrepository.h"

#include <QMap>

namespace fakes {

// global_configurations en memoria. Aplica los cambios como el adaptador:
// todo o nada, y un valor que ya no es el esperado es un conflicto.
class InMemorySettingsRepository final : public application::SettingsRepository
{
public:
    QMap<domain::SystemParameter, application::SystemParameterDto> rows;
    // Si no está vacío, leer y guardar "fallan" con este error.
    QString failure;
    int reads = 0;
    QList<QList<Change>> updates;
    QDateTime now = QDateTime(QDate(2026, 11, 25), QTime(10, 0));

    void store(domain::SystemParameter parameter, qint64 hundredths,
               const QDateTime &updatedAt = QDateTime(QDate(2026, 1, 9), QTime(9, 0)))
    {
        rows.insert(parameter, {parameter, hundredths, updatedAt});
    }

    QList<application::SystemParameterDto> getSystemParameters(QString *error) override
    {
        ++reads;
        if (!failure.isEmpty()) {
            if (error)
                *error = failure;
            return {};
        }
        if (error)
            error->clear();
        return rows.values();
    }

    UpdateOutcome updateSystemParameters(const QList<Change> &changes) override
    {
        updates << changes;
        UpdateOutcome outcome;
        if (!failure.isEmpty()) {
            outcome.status = UpdateOutcome::Status::Failed;
            outcome.errorMessage = failure;
            return outcome;
        }
        for (const Change &change : changes) {
            const auto row = rows.constFind(change.parameter);
            const std::optional<qint64> current =
                row == rows.cend() ? std::nullopt : row->hundredths;
            if (current != change.expected) {
                outcome.status = UpdateOutcome::Status::Conflict;
                return outcome;
            }
        }
        for (const Change &change : changes)
            rows.insert(change.parameter, {change.parameter, change.value, now});
        outcome.status = UpdateOutcome::Status::Updated;
        return outcome;
    }
};

} // namespace fakes

#endif // TESTS_SUPPORT_SETTINGSFAKES_H
