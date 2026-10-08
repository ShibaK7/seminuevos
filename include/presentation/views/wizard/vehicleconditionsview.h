#ifndef PRESENTATION_VIEWS_WIZARD_VEHICLECONDITIONSVIEW_H
#define PRESENTATION_VIEWS_WIZARD_VEHICLECONDITIONSVIEW_H

#include "application/dto/catalogdtos.h"
#include "application/dto/registrationdtos.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;
class QVBoxLayout;
class ConditionChecklistRow;

// Paso 2 del wizard: especificaciones básicas (izquierda) + checklist de
// condición agrupado (derecha). Los renglones del checklist se leen de
// vehicle_conditions_cat, así que agregar o renombrar un elemento no
// requiere recompilar.
//
// Las especificaciones básicas son obligatorias (combustible, cilindros,
// transmisión, interiores, cristales y aire acondicionado), pero cuáles y con
// qué reglas
// lo decide el dominio, no esta vista: entrega lo capturado como
// VehicleConditionsDto y el asistente se lo pasa al servicio, igual que el Paso 1. El checklist, en cambio, no tiene mínimos --
// consistente con el post-it del wireframe original ("no hay mínimos
// definidos").
class VehicleConditionsView : public QWidget
{
    Q_OBJECT

public:
    explicit VehicleConditionsView(QWidget *parent = nullptr);

    // Llena el combo de combustible y construye el checklist con los catálogos
    // que el asistente leyó por el servicio. Se llama una vez, al llegar.
    void setLookups(const application::RegistrationLookupsDto &lookups);

    // Lo capturado. Un combo sin elegir queda vacío (std::optional) para que
    // el dominio lo reporte como faltante en vez de recibir un valor que nadie
    // capturó. Solo viajan las filas marcadas del checklist.
    application::VehicleConditionsDto conditions() const;

private:
    QWidget *buildBasicSpecsPanel();
    QWidget *buildChecklistPanel();
    void populateChecklist(const QList<application::ChecklistItemDto> &items);
    // Mensaje visible en el panel del checklist. Sin esto, un fallo de la
    // consulta dejaba media pantalla en blanco sin explicación.
    void showChecklistMessage(const QString &message);
    void markAllInGroup(const QString &category, bool checked);

    QComboBox *m_fuelTypeCombo;
    QComboBox *m_cylindersCombo;
    QComboBox *m_transmissionCombo;
    QComboBox *m_interiorMaterialCombo;
    QComboBox *m_windowRegulatorsCombo;
    QComboBox *m_airConditioningCombo;

    QWidget *m_checklistContent = nullptr;
    QVBoxLayout *m_checklistLayout = nullptr;
    QLabel *m_checklistMessageLabel = nullptr;
    QList<ConditionChecklistRow *> m_checklistRows;
};

#endif // PRESENTATION_VIEWS_WIZARD_VEHICLECONDITIONSVIEW_H
