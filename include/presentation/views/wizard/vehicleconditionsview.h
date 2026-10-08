#ifndef PRESENTATION_VIEWS_WIZARD_VEHICLECONDITIONSVIEW_H
#define PRESENTATION_VIEWS_WIZARD_VEHICLECONDITIONSVIEW_H

#include "application/dto/catalogdtos.h"
#include "application/dto/registrationdtos.h"
#include "presentation/presenters/ivehicleconditionsview.h"

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class VehicleConditionsView;
}
QT_END_NAMESPACE

class ConditionChecklistRow;

// Paso 2 del wizard: especificaciones básicas (izquierda) + checklist de
// condición agrupado (derecha). Los renglones del checklist se leen de
// vehicle_conditions_cat, así que agregar o renombrar un elemento no
// requiere recompilar.
//
// Las dos tarjetas, los seis combos de especificaciones y el contenedor del
// checklist están en src/ui/vehicleconditionsview.ui. Las filas del checklist
// (ConditionChecklistRow) dependen del catálogo y se arman en código, en
// setChecklist().
//
// Las especificaciones básicas son obligatorias (combustible, cilindros,
// transmisión, interiores, cristales y aire acondicionado), pero cuáles y con
// qué reglas
// lo decide el dominio, no esta vista: es pasiva (IVehicleConditionsView),
// entrega lo capturado como VehicleConditionsDto y VehicleConditionsPresenter
// se lo pasa al servicio, igual que el Paso 1. El checklist, en cambio, no tiene mínimos --
// consistente con el post-it del wireframe original ("no hay mínimos
// definidos").
class VehicleConditionsView : public QWidget, public presentation::IVehicleConditionsView
{
    Q_OBJECT

public:
    explicit VehicleConditionsView(QWidget *parent = nullptr);
    ~VehicleConditionsView() override;

    // Lo capturado. Un combo sin elegir queda vacío (std::optional) para que
    // el dominio lo reporte como faltante en vez de recibir un valor que nadie
    // capturó. Solo viajan las filas marcadas del checklist.
    application::VehicleConditionsDto conditions() const override;

    void setFuelTypes(const QList<application::CatalogOptionDto> &fuelTypes) override;
    // Construye el checklist. Se llama una vez, al llegar los catálogos.
    void setChecklist(const QList<application::ChecklistItemDto> &items) override;
    // Mensaje visible en el panel del checklist. Sin esto, un fallo de la
    // consulta dejaba media pantalla en blanco sin explicación.
    void showChecklistMessage(const QString &message) override;
    void showFieldErrors(const QList<domain::ValidationError> &errors) override;
    bool focusField(const QString &field) override;

signals:
    // Cambió cualquier campo, incluidas las filas del checklist.
    void edited();

private:
    void markAllInGroup(const QString &category, bool checked);

    Ui::VehicleConditionsView *ui;
    QList<ConditionChecklistRow *> m_checklistRows;
};

#endif // PRESENTATION_VIEWS_WIZARD_VEHICLECONDITIONSVIEW_H
