#ifndef PRESENTATION_SETTINGS_SYSTEMSETTINGSPRESENTER_H
#define PRESENTATION_SETTINGS_SYSTEMSETTINGSPRESENTER_H

#include "application/auth/dto/authdtos.h"
#include "application/settings/dto/settingsdtos.h"
#include "domain/common/value_objects/enums.h"
#include "presentation/common/navigation/editsession.h"

#include <QObject>

#include <optional>

namespace application {
class SystemSettingsService;
}

namespace presentation {

class ISystemSettingsView;
class TaskRunner;

// Presenter de los parámetros de Configuraciones (US-07.1, #82): los lee, los
// muestra de solo lectura, y con "Editar" deja corregirlos y guardarlos. Lo
// que toca la base corre fuera del hilo de la interfaz.
//
// Sin widgets (solo QtCore): se prueba con una vista falsa.
class SystemSettingsPresenter final : public QObject
{
    Q_OBJECT

public:
    SystemSettingsPresenter(ISystemSettingsView &view,
                            const application::SystemSettingsService &service, TaskRunner &runner,
                            const application::SessionDto &actor, QObject *parent = nullptr);

    // Lee y pinta los parámetros.
    void start();
    // Vuelve a leerlos, salvo a media edición: no pisa lo capturado.
    void refresh();

    void onEdit();
    // Cambió un campo (lo avisa la vista).
    void onEdited();
    // Guarda lo que cambió. Sin cambios, solo vuelve a consulta.
    void onSave();
    // Vuelve a lo leído; si hay cambios, pregunta antes.
    void onCancel();

    // Si se puede salir: pregunta si hay cambios sin guardar, y mientras
    // guarda pide esperar.
    bool confirmLeave();

private:
    void load();
    void paint();
    void enterView();

    ISystemSettingsView &m_view;
    const application::SystemSettingsService &m_service;
    TaskRunner &m_runner;
    const application::SessionDto m_actor;
    EditSession m_session;
    // Lo leído, que es lo esperado al guardar.
    QList<application::SystemParameterDto> m_loaded;
    // Una relectura que llegó a media edición.
    std::optional<QList<application::SystemParameterDto>> m_pending;
    bool m_loading = false;
    bool m_saving = false;
    // Mientras pinta, los cambios de los campos no son del usuario.
    bool m_painting = false;
    // Cada lectura lleva un número; solo se pinta la última.
    int m_loadGeneration = 0;
};

} // namespace presentation

#endif // PRESENTATION_SETTINGS_SYSTEMSETTINGSPRESENTER_H
