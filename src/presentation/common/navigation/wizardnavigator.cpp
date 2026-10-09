#include "presentation/common/navigation/wizardnavigator.h"

#include <algorithm>

namespace presentation {

// Un conteo negativo se toma como cero en lugar de tronar: el navegador queda
// sin pasos y todas las consultas caen en el caso fuera de rango.
WizardNavigator::WizardNavigator(int stepCount, bool freeNavigation)
    : m_steps(std::max(stepCount, 0))
    , m_freeNavigation(freeNavigation)
{
}

int WizardNavigator::stepCount() const
{
    return static_cast<int>(m_steps.size());
}

int WizardNavigator::current() const
{
    return m_current;
}

bool WizardNavigator::freeNavigation() const
{
    return m_freeNavigation;
}

bool WizardNavigator::contains(int step) const
{
    return step >= 0 && step < m_steps.size();
}

void WizardNavigator::setValid(int step, bool valid)
{
    if (contains(step))
        m_steps[step].valid = valid;
}

bool WizardNavigator::isValid(int step) const
{
    return contains(step) && m_steps.at(step).valid;
}

void WizardNavigator::markAttempted(int step)
{
    if (contains(step))
        m_steps[step].attempted = true;
}

void WizardNavigator::markAllAttempted()
{
    for (StepState &state : m_steps)
        state.attempted = true;
}

bool WizardNavigator::isAttempted(int step) const
{
    return contains(step) && m_steps.at(step).attempted;
}

bool WizardNavigator::canEnter(int target) const
{
    if (!contains(target))
        return false;
    if (m_freeNavigation || target <= m_current)
        return true;
    return firstBlockingStep(target) < 0;
}

int WizardNavigator::firstBlockingStep(int target) const
{
    const int end = std::min(target, stepCount());
    for (int step = 0; step < end; ++step) {
        if (!m_steps.at(step).valid)
            return step;
    }
    return -1;
}

bool WizardNavigator::goTo(int target)
{
    if (target == m_current || !canEnter(target))
        return false;
    m_current = target;
    return true;
}

void WizardNavigator::moveTo(int target)
{
    if (contains(target))
        m_current = target;
}

bool WizardNavigator::isComplete(int step) const
{
    return contains(step) && m_steps.at(step).attempted && m_steps.at(step).valid;
}

StepVisual WizardNavigator::visual(int step) const
{
    if (!contains(step))
        return StepVisual::Locked;
    // El paso actual siempre se ve como actual, aunque tenga errores: el
    // usuario ya está ahí, y los errores se le muestran en sus campos.
    if (step == m_current)
        return StepVisual::Current;
    // Bloqueado gana a Error: si no se puede entrar, lo primero que hay que
    // decir es eso, no qué le falta a un paso que todavía no se alcanza.
    if (!canEnter(step))
        return StepVisual::Locked;
    if (m_steps.at(step).attempted && !m_steps.at(step).valid)
        return StepVisual::Error;
    if (isComplete(step))
        return StepVisual::Done;
    return StepVisual::Pending;
}

bool WizardNavigator::allValid() const
{
    return std::all_of(m_steps.cbegin(), m_steps.cend(),
                       [](const StepState &state) { return state.valid; });
}

} // namespace presentation
