#ifndef PRESENTATION_COMMON_NAVIGATION_WIZARDNAVIGATOR_H
#define PRESENTATION_COMMON_NAVIGATION_WIZARDNAVIGATOR_H

#include <QList>

namespace presentation {

// Cómo se ve un paso en el stepper. Lo calcula WizardNavigator::visual() con
// esta prioridad: Current > Locked > Error > Done > Pending.
enum class StepVisual { Pending, Current, Done, Error, Locked };

// Estado de navegación de un asistente por pasos: cuál se muestra, cuáles son
// válidos, cuáles ya se intentaron y, a partir de eso, a cuáles se puede
// entrar y cuáles llevan palomita.
//
// Es estado de presentación, no de dominio: si los datos de un paso son
// válidos lo decide el dominio, y esta clase solo recuerda ese veredicto para
// decidir la navegación. Vive aquí, sin widgets (solo QtCore), para poder
// probarla sin levantar ninguna pantalla. Hoy la usa VehicleWizardView; más
// adelante el presenter del asistente la va a usar tal cual.
//
// Las reglas:
//   - Regresar siempre se puede. Avanzar al paso k, solo si todos los pasos
//     anteriores a k son válidos.
//   - "Intentado" significa que el usuario ya quiso avanzar desde ese paso o
//     guardar. Antes de eso, un paso inválido no se marca como error: nadie
//     quiere ver en rojo un formulario que apenas abrió.
//   - La palomita significa "intentado y válido ahora". No se queda puesta:
//     se apaga sola si el paso deja de ser válido y vuelve cuando se corrige.
//     Por eso no hay un "paso más lejano desbloqueado" que solo crezca.
//
// Un índice fuera de rango nunca truena: las consultas responden false (o -1)
// y los cambios no hacen nada.
class WizardNavigator
{
public:
    // freeNavigation deja entrar a cualquier paso sin importar si los
    // anteriores son válidos. Es solo para desarrollo (WIZARD_FREE_NAVIGATION
    // en el .env): permite trabajar los estilos de un paso sin capturar los
    // anteriores, en vez de comentar la validación.
    explicit WizardNavigator(int stepCount, bool freeNavigation = false);

    int stepCount() const;
    int current() const;
    bool freeNavigation() const;

    // Guarda el veredicto de la última validación del paso. Quien valida es
    // el dominio; aquí solo se recuerda el resultado.
    void setValid(int step, bool valid);
    bool isValid(int step) const;

    void markAttempted(int step);
    void markAllAttempted();
    bool isAttempted(int step) const;

    // Si se puede entrar a `target`: con navegación libre, siempre; si no, si
    // es el paso actual o uno anterior, o si todos los pasos anteriores a él
    // son válidos. Los pasos posteriores a `target` no cuentan.
    bool canEnter(int target) const;

    // El primer paso anterior a `target` que no es válido, o -1 si no hay
    // ninguno. Sirve para explicar un bloqueo ("Completa «Detalles» para
    // continuar"). Solo mira la validez, no la navegación libre: dice qué
    // falta, no si se puede pasar. Un `target` más allá del último paso
    // revisa todos los pasos.
    int firstBlockingStep(int target) const;

    // Va a `target` solo si canEnter(target). Devuelve si de verdad se movió,
    // así que pedir el paso actual devuelve false.
    bool goTo(int target);

    // Va a `target` sin preguntar. Es para regresar y para llevar al usuario
    // al paso que tiene errores. Un índice fuera de rango no hace nada.
    void moveTo(int target);

    // La palomita: el paso ya se intentó y es válido ahora.
    bool isComplete(int step) const;

    // Cómo pintar el paso. Un índice fuera de rango se reporta como Locked,
    // igual que canEnter(), que dice que no se puede entrar.
    StepVisual visual(int step) const;

    bool allValid() const;

private:
    bool contains(int step) const;

    struct StepState
    {
        bool valid = false;
        bool attempted = false;
    };

    QList<StepState> m_steps;
    int m_current = 0;
    bool m_freeNavigation = false;
};

} // namespace presentation

#endif // PRESENTATION_COMMON_NAVIGATION_WIZARDNAVIGATOR_H
