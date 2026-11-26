#include "presentation/settings/systemsettingspresenter.h"

#include "application/settings/services/systemsettingsservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/settings/isystemsettingsview.h"

#include <QScopedValueRollback>

namespace presentation {

SystemSettingsPresenter::SystemSettingsPresenter(ISystemSettingsView &view,
                                                 const application::SystemSettingsService &service,
                                                 TaskRunner &runner, domain::UserRole role,
                                                 QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_service(service)
    , m_runner(runner)
    , m_role(role)
{
}

void SystemSettingsPresenter::start()
{
    m_view.showMode(FormMode::View, false);
    load();
}

void SystemSettingsPresenter::refresh()
{
    if (!m_session.isEditing() && !m_saving)
        load();
}

void SystemSettingsPresenter::load()
{
    const int generation = ++m_loadGeneration;
    m_loading = true;
    m_view.setBusy(true);
    const application::SystemSettingsService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, role] { return service->getSystemParameters(role); },
        [this, generation](const application::SystemParametersResult &result) {
            using Status = application::SystemParametersResult::Status;
            if (generation != m_loadGeneration)
                return;
            m_loading = false;
            m_view.setBusy(false);
            switch (result.status) {
            case Status::Loaded:
                m_session.allowEditing(true);
                // Una relectura que llega a media edición no pisa lo
                // capturado ni lo leído: al guardar, lo esperado tiene que
                // ser lo que el usuario vio. Se aplica al volver a consulta.
                if (m_session.isEditing()) {
                    m_pending = result.parameters;
                } else {
                    m_loaded = result.parameters;
                    paint();
                }
                break;
            case Status::Denied:
                m_view.showMessage(QStringLiteral("Tu rol no puede ver los parámetros del sistema."));
                break;
            case Status::Failed:
                m_view.showMessage(QStringLiteral("No se pudieron leer los parámetros: %1")
                                       .arg(result.errorMessage));
                break;
            }
            if (!m_session.isEditing())
                m_view.showMode(FormMode::View, m_session.canEdit());
        });
}

void SystemSettingsPresenter::paint()
{
    const QScopedValueRollback<bool> painting(m_painting, true);
    m_view.showParameters(m_loaded);
}

void SystemSettingsPresenter::enterView()
{
    m_session.endEdit();
    if (m_pending) {
        m_loaded = *m_pending;
        m_pending.reset();
    }
    m_view.showFieldErrors({});
    paint();
    m_view.showMode(FormMode::View, m_session.canEdit());
}

void SystemSettingsPresenter::onEdit()
{
    if (m_loading || m_saving || !m_session.beginEdit())
        return;
    m_view.showMessage(QString());
    m_view.showNotice(QString());
    m_view.showMode(FormMode::Edit, true);
}

void SystemSettingsPresenter::onEdited()
{
    if (m_session.isEditing() && !m_painting)
        m_session.markEdited();
}

void SystemSettingsPresenter::onCancel()
{
    if (!m_session.isEditing() || m_saving)
        return;
    if (m_session.isDirty() && !m_view.confirmDiscardChanges())
        return;
    m_view.showMessage(QString());
    enterView();
}

void SystemSettingsPresenter::onSave()
{
    if (!m_session.isEditing() || m_saving)
        return;
    m_view.showMessage(QString());
    if (!m_session.isDirty()) {
        enterView();
        m_view.showNotice(QStringLiteral("No hubo cambios que guardar."));
        return;
    }

    // Lo esperado sale de lo leído, no de la pantalla.
    QList<application::SystemParameterEditDto> edits;
    for (const auto &[parameter, value] : m_view.editedValues()) {
        application::SystemParameterEditDto edit;
        edit.parameter = parameter;
        edit.edited = value;
        for (const application::SystemParameterDto &loaded : std::as_const(m_loaded)) {
            if (loaded.parameter == parameter)
                edit.loaded = loaded.hundredths;
        }
        edits << edit;
    }

    m_saving = true;
    m_view.setBusy(true);
    const application::SystemSettingsService *service = &m_service;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [service, edits, role] { return service->updateSystemParameters(edits, role); },
        [this](const application::SystemParametersUpdateResult &result) {
            using Status = application::SystemParametersUpdateResult::Status;
            m_saving = false;
            m_view.setBusy(false);
            switch (result.status) {
            case Status::Saved:
                enterView();
                m_view.showNotice(QStringLiteral("Cambios guardados."));
                // Se relee para mostrar la fecha nueva de lo que cambió.
                load();
                break;
            case Status::NoChanges:
                enterView();
                m_view.showNotice(QStringLiteral("No hubo cambios que guardar."));
                break;
            case Status::Rejected:
                m_view.showFieldErrors(result.validation.errors());
                m_view.showMessage(QStringLiteral("Revisa los campos marcados."));
                break;
            case Status::Conflict:
                enterView();
                m_view.showMessage(QStringLiteral("Otro administrador cambió los parámetros mientras "
                                                  "editabas; se cargaron los vigentes. Vuelve a "
                                                  "capturar tus cambios."));
                load();
                break;
            case Status::Denied:
                enterView();
                m_view.showMessage(QStringLiteral("Tu rol no puede cambiar los parámetros del sistema."));
                break;
            case Status::Failed:
                // Se queda en edición con lo capturado, para reintentar.
                m_view.showMessage(QStringLiteral("No se guardaron los cambios: %1")
                                       .arg(result.errorMessage));
                break;
            }
        });
}

bool SystemSettingsPresenter::confirmLeave()
{
    if (m_saving) {
        m_view.showNotice(QStringLiteral("Espera a que terminen de guardarse los parámetros."));
        return false;
    }
    if (!m_session.needsConfirmationToLeave())
        return true;
    if (!m_view.confirmDiscardChanges())
        return false;
    enterView();
    return true;
}

} // namespace presentation
