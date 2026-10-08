#ifndef PRESENTATION_VIEWS_COMPONENTS_WIZARDSTEPPER_H
#define PRESENTATION_VIEWS_COMPONENTS_WIZARDSTEPPER_H

#include <QList>
#include <QStringList>
#include <QWidget>

class QHBoxLayout;
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
//
// Se puede promover en Qt Designer (clase base QWidget) con la propiedad
// dinámica `steps` (StringList). Los estados de los pasos son solo de
// ejecución: no se declaran en el .ui.
class WizardStepper : public QWidget
{
    Q_OBJECT
    // Títulos de los pasos, en orden. Q_PROPERTY para que el .ui pueda
    // declararlos: uic los aplica con setProperty("steps", ...).
    Q_PROPERTY(QStringList steps READ steps WRITE setSteps)

public:
    // El que usa uic con el widget promovido: arranca sin pasos.
    explicit WizardStepper(QWidget *parent = nullptr);
    explicit WizardStepper(const QStringList &stepLabels, QWidget *parent = nullptr);

    QStringList steps() const;
    // Reconstruye la fila con un botón por título: el primero como "current" y
    // los demás "pending", todos sin palomita. Lo que se haya pintado antes con
    // setStepState() se pierde y hay que volver a pintarlo.
    void setSteps(const QStringList &stepLabels);

    // Pinta el paso `index`. `state` es uno de los estados de arriba;
    // `complete` muestra u oculta la palomita; `hint` es el tooltip de un paso
    // bloqueado (en los demás estados se ignora y el paso queda sin tooltip).
    // Un índice fuera de rango no hace nada.
    void setStepState(int index, const QString &state, bool complete, const QString &hint);
    bool isStepCompleted(int index) const;

signals:
    void stepClicked(int index);

private:
    QHBoxLayout *m_row;
    QStringList m_labels;
    QList<QPushButton *> m_buttons;
    QList<QLabel *> m_textLabels;
    QList<QLabel *> m_iconLabels;
    QList<bool> m_completed;
};

#endif // PRESENTATION_VIEWS_COMPONENTS_WIZARDSTEPPER_H
