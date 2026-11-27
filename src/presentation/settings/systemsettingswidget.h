#ifndef PRESENTATION_SETTINGS_SYSTEMSETTINGSWIDGET_H
#define PRESENTATION_SETTINGS_SYSTEMSETTINGSWIDGET_H

#include "presentation/settings/isystemsettingsview.h"

#include <QMap>
#include <QWidget>

#include <optional>

QT_BEGIN_NAMESPACE
namespace Ui {
    class SystemSettingsWidget;
}
QT_END_NAMESPACE

class QAbstractSpinBox;
class QLabel;

namespace presentation {
class SystemSettingsPresenter;
}

// La tarjeta de los parámetros de Configuraciones (US-07.1, #82): Valor 1
// UMA y Máximo valor pagos en efectivo, cada uno con su "Actualizado el ...".
// Los saldos tienen su propia tarjeta (CashBalancesWidget). Su formulario es
// systemsettingswidget.ui, en esta misma carpeta.
//
// Vista pasiva de SystemSettingsPresenter: de solo lectura hasta "Editar",
// que muestra "Guardar" y "Cancelar". Cada campo lleva en "field" la clave con
// que el dominio reporta su error.
class SystemSettingsWidget : public QWidget, public presentation::ISystemSettingsView
{
    Q_OBJECT

public:
    explicit SystemSettingsWidget(QWidget *parent = nullptr);
    ~SystemSettingsWidget() override;

    void bind(presentation::SystemSettingsPresenter &presenter);

    // --- ISystemSettingsView ---
    void setBusy(bool busy) override;
    void showParameters(const QList<application::SystemParameterDto> &parameters) override;
    QList<EditedValue> editedValues() const override;
    void showMode(presentation::FormMode mode, bool canEdit) override;
    void showFieldErrors(const QList<domain::ValidationError> &errors) override;
    void showMessage(const QString &message) override;
    void showNotice(const QString &notice) override;
    bool confirmDiscardChanges() override;

signals:
    void editRequested();
    void saveRequested();
    void cancelRequested();
    // Cambió cualquier campo.
    void edited();

private:
    QAbstractSpinBox *fieldOf(domain::SystemParameter parameter) const;
    QLabel *updatedLabelOf(domain::SystemParameter parameter) const;
    // Lo que el campo dice ahora, en centésimos (la UMA en cero: nullopt).
    std::optional<qint64> fieldValue(domain::SystemParameter parameter) const;

    // Lo que se pintó en cada campo: el valor guardado y cómo quedó en el
    // campo. Un campo que nadie tocó devuelve el guardado tal cual, aunque
    // el campo lo haya redondeado (un tope de 3200.50 en un campo entero).
    struct Painted
    {
        std::optional<qint64> stored;
        std::optional<qint64> shown;
    };
    QMap<domain::SystemParameter, Painted> m_painted;

    Ui::SystemSettingsWidget *ui;
    presentation::FormMode m_mode = presentation::FormMode::View;
    bool m_busy = false;
    bool m_canEdit = false;
};

#endif // PRESENTATION_SETTINGS_SYSTEMSETTINGSWIDGET_H
