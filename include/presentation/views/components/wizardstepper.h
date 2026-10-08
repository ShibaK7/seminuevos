#ifndef PRESENTATION_VIEWS_COMPONENTS_WIZARDSTEPPER_H
#define PRESENTATION_VIEWS_COMPONENTS_WIZARDSTEPPER_H

#include <QList>
#include <QStringList>
#include <QWidget>

class QPushButton;
class QLabel;

// Fila de navegación superior del wizard (Detalles / Condición / Archivos).
// Cada paso se pinta según su estado, que viaja en la propiedad dinámica
// stepState del botón y de su texto para que lo lea el QSS -- ver
// global-style-clean.qss, sección "Wizard" (QPushButton[class="wizard-step"]
// / QLabel[class="wizard-step-text"]). Los estados son:
//   - "current": el paso que se muestra (subrayado);
//   - "done": intentado y válido;
//   - "error": intentado y con datos que corregir;
//   - "pending": se puede abrir, pero todavía no se intenta;
//   - "locked": no se puede abrir hasta completar un paso anterior. Lleva
//     cursor de prohibido y un tooltip que dice qué falta.
//
// La palomita va aparte del estado, en `complete` de setStepState(): el paso
// actual también la lleva si ya se intentó y es válido, aunque se pinte como
// "current". El texto y el ícono se arman a mano como QLabel hijos del botón
// (en vez de QPushButton::setText/setIcon) para controlar el espacio entre
// ambos y su alineación vertical.
//
// No decide nada por sí mismo: qué estado tiene cada paso lo calcula
// VehicleWizardView con su WizardNavigator, y un clic solo se avisa con
// stepClicked(). Los pasos bloqueados también lo emiten: el wizard aprovecha
// el clic para explicar por qué no se puede pasar, en vez de que el clic se
// pierda sin respuesta.
class WizardStepper : public QWidget
{
    Q_OBJECT

public:
    explicit WizardStepper(const QStringList &stepLabels, QWidget *parent = nullptr);

    // Pinta el paso `index`. `state` es uno de los estados de arriba;
    // `complete` muestra u oculta la palomita; `hint` es el tooltip de un paso
    // bloqueado (en los demás estados se ignora y el paso queda sin tooltip).
    // Un índice fuera de rango no hace nada.
    void setStepState(int index, const QString &state, bool complete, const QString &hint);
    bool isStepCompleted(int index) const;

signals:
    void stepClicked(int index);

private:
    QStringList m_labels;
    QList<QPushButton *> m_buttons;
    QList<QLabel *> m_textLabels;
    QList<QLabel *> m_iconLabels;
    QList<bool> m_completed;
};

#endif // PRESENTATION_VIEWS_COMPONENTS_WIZARDSTEPPER_H
