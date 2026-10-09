#include "presentation/common/navigation/wizardnavigator.h"

#include <QObject>
#include <QTest>

namespace presentation {

// QCOMPARE imprime como número un enum que no está registrado con Q_ENUM.
// QtTest encuentra esta sobrecarga por ADL (vive en el mismo namespace que
// StepVisual), así que cuando falla una comparación de visual() el mensaje
// dice "Locked" en vez de "4".
char *toString(StepVisual visual)
{
    switch (visual) {
    case StepVisual::Pending:
        return qstrdup("Pending");
    case StepVisual::Current:
        return qstrdup("Current");
    case StepVisual::Done:
        return qstrdup("Done");
    case StepVisual::Error:
        return qstrdup("Error");
    case StepVisual::Locked:
        return qstrdup("Locked");
    }
    return qstrdup("?");
}

} // namespace presentation

using presentation::StepVisual;
using presentation::WizardNavigator;

// Modelo de navegación del asistente de registro: atrás libre, adelante solo
// con todos los pasos anteriores válidos, y palomita = "intentado y válido
// ahora". Los casos usan 3 pasos, como el asistente, salvo donde hace falta
// uno más para separar dos reglas.
class TestWizardNavigator : public QObject
{
    Q_OBJECT

private slots:
    void startsOnFirstStepWithLaterStepsLocked();
    void cannotAdvancePastInvalidStep();
    void canAlwaysGoBack();
    void laterStepsDoNotBlockEarlierOnes();
    void checkmarkOnlyAfterAttempt();
    void checkmarkFollowsValidity();
    void firstBlockingStepReportsFirstInvalid();
    void goToReportsWhetherItMoved();
    void moveToIgnoresGating();
    void freeNavigationEntersAnyStep();
    void visualPriorities();
    void markAllAttemptedMarksEveryStep();
    void allValidRequiresEveryStep();
    void outOfRangeIndicesAreIgnored();
};

void TestWizardNavigator::startsOnFirstStepWithLaterStepsLocked()
{
    WizardNavigator nav(3);

    QCOMPARE(nav.stepCount(), 3);
    QCOMPARE(nav.current(), 0);
    QVERIFY(!nav.freeNavigation());
    QCOMPARE(nav.visual(0), StepVisual::Current);
    QCOMPARE(nav.visual(1), StepVisual::Locked);
    QCOMPARE(nav.visual(2), StepVisual::Locked);
    for (int step = 0; step < nav.stepCount(); ++step) {
        QVERIFY(!nav.isValid(step));
        QVERIFY(!nav.isAttempted(step));
        QVERIFY(!nav.isComplete(step));
    }
}

void TestWizardNavigator::cannotAdvancePastInvalidStep()
{
    WizardNavigator nav(3);

    QVERIFY(!nav.canEnter(1));
    QVERIFY(!nav.goTo(1));
    QCOMPARE(nav.current(), 0);

    nav.setValid(0, true);
    QVERIFY(nav.canEnter(1));
    // El paso 2 exige también el 1, no solo el actual.
    QVERIFY(!nav.canEnter(2));
    QVERIFY(nav.goTo(1));
    QCOMPARE(nav.current(), 1);
}

void TestWizardNavigator::canAlwaysGoBack()
{
    WizardNavigator nav(3);
    nav.setValid(0, true);
    nav.setValid(1, true);
    QVERIFY(nav.goTo(2));

    // Aunque los pasos anteriores dejen de ser válidos, regresar no se bloquea.
    nav.setValid(0, false);
    nav.setValid(1, false);
    QVERIFY(nav.canEnter(1));
    QVERIFY(nav.canEnter(0));
    QVERIFY(nav.goTo(1));
    QCOMPARE(nav.current(), 1);
    QVERIFY(nav.goTo(0));
    QCOMPARE(nav.current(), 0);

    // Ya atrás, volver a avanzar sí exige que sean válidos: no hay un
    // "desbloqueado" que se conserve.
    QVERIFY(!nav.canEnter(1));
    QVERIFY(!nav.canEnter(2));
}

void TestWizardNavigator::laterStepsDoNotBlockEarlierOnes()
{
    WizardNavigator nav(3);
    nav.setValid(0, true);
    nav.setValid(1, true);

    // El paso 2 es inválido, pero para entrar a él solo cuentan los anteriores.
    QVERIFY(nav.canEnter(2));
    QCOMPARE(nav.visual(2), StepVisual::Pending);
}

void TestWizardNavigator::checkmarkOnlyAfterAttempt()
{
    WizardNavigator nav(3);

    // Válido pero sin intentar: todavía sin palomita.
    nav.setValid(0, true);
    QVERIFY(!nav.isComplete(0));

    nav.markAttempted(0);
    QVERIFY(nav.isAttempted(0));
    QVERIFY(nav.isComplete(0));

    // Intentado pero inválido tampoco la lleva.
    nav.markAttempted(1);
    QVERIFY(!nav.isComplete(1));
}

void TestWizardNavigator::checkmarkFollowsValidity()
{
    WizardNavigator nav(3);
    nav.setValid(0, true);
    nav.markAttempted(0);
    QVERIFY(nav.goTo(1));
    QVERIFY(nav.isComplete(0));
    QCOMPARE(nav.visual(0), StepVisual::Done);

    // Se apaga sola cuando el paso deja de ser válido...
    nav.setValid(0, false);
    QVERIFY(!nav.isComplete(0));
    QCOMPARE(nav.visual(0), StepVisual::Error);

    // ...y vuelve en cuanto se corrige, sin tener que intentarlo otra vez.
    nav.setValid(0, true);
    QVERIFY(nav.isComplete(0));
    QCOMPARE(nav.visual(0), StepVisual::Done);
}

void TestWizardNavigator::firstBlockingStepReportsFirstInvalid()
{
    WizardNavigator nav(3);

    // Antes del primer paso no hay nada que bloquee.
    QCOMPARE(nav.firstBlockingStep(0), -1);
    QCOMPARE(nav.firstBlockingStep(1), 0);
    QCOMPARE(nav.firstBlockingStep(2), 0);

    nav.setValid(0, true);
    QCOMPARE(nav.firstBlockingStep(1), -1);
    QCOMPARE(nav.firstBlockingStep(2), 1);

    nav.setValid(1, true);
    QCOMPARE(nav.firstBlockingStep(2), -1);
    // Más allá del último paso se revisan todos: falta el 2.
    QCOMPARE(nav.firstBlockingStep(3), 2);

    // Si hay varios inválidos, reporta el primero.
    nav.setValid(0, false);
    QCOMPARE(nav.firstBlockingStep(3), 0);
}

void TestWizardNavigator::goToReportsWhetherItMoved()
{
    WizardNavigator nav(3);
    nav.setValid(0, true);

    // Bloqueado: no se mueve y lo dice.
    QVERIFY(!nav.goTo(2));
    QCOMPARE(nav.current(), 0);

    // Pedir el paso actual no es moverse.
    QVERIFY(!nav.goTo(0));
    QCOMPARE(nav.current(), 0);

    QVERIFY(nav.goTo(1));
    QCOMPARE(nav.current(), 1);
}

void TestWizardNavigator::moveToIgnoresGating()
{
    WizardNavigator nav(3);

    // Ningún paso es válido, pero moveTo() es forzado: es el que usa el
    // asistente para llevar al usuario al paso que tiene errores.
    nav.moveTo(2);
    QCOMPARE(nav.current(), 2);
    nav.moveTo(0);
    QCOMPARE(nav.current(), 0);
}

void TestWizardNavigator::freeNavigationEntersAnyStep()
{
    WizardNavigator nav(3, true);

    QVERIFY(nav.freeNavigation());
    QVERIFY(nav.canEnter(2));
    QCOMPARE(nav.visual(1), StepVisual::Pending);
    QCOMPARE(nav.visual(2), StepVisual::Pending);
    QVERIFY(nav.goTo(2));
    QCOMPARE(nav.current(), 2);

    // La validez se sigue llevando aunque no bloquee: firstBlockingStep() dice
    // qué falta, y un paso intentado e inválido se sigue viendo como error.
    QCOMPARE(nav.firstBlockingStep(2), 0);
    nav.markAttempted(0);
    QCOMPARE(nav.visual(0), StepVisual::Error);

    // Un paso que no existe sigue sin poder abrirse.
    QVERIFY(!nav.canEnter(3));
}

void TestWizardNavigator::visualPriorities()
{
    WizardNavigator nav(4);

    // Current gana a Error: el paso actual intentado e inválido se ve como actual.
    nav.markAttempted(0);
    QCOMPARE(nav.visual(0), StepVisual::Current);

    // Locked gana a Error: el paso 2 está intentado e inválido, pero el 1 lo
    // bloquea, y eso es lo primero que hay que decir.
    nav.setValid(0, true);
    QVERIFY(nav.goTo(1));
    nav.markAttempted(2);
    QCOMPARE(nav.visual(2), StepVisual::Locked);

    // Con el 1 válido, el 2 ya se alcanza y aparece su error; el 3 queda
    // bloqueado por el 2.
    nav.setValid(1, true);
    QCOMPARE(nav.visual(2), StepVisual::Error);
    QCOMPARE(nav.visual(3), StepVisual::Locked);

    // Done: intentado y válido.
    QCOMPARE(nav.visual(0), StepVisual::Done);
    nav.setValid(2, true);
    QCOMPARE(nav.visual(2), StepVisual::Done);

    // Pending: alcanzable y sin intentar, sea válido o no.
    QCOMPARE(nav.visual(3), StepVisual::Pending);
    nav.setValid(3, true);
    QCOMPARE(nav.visual(3), StepVisual::Pending);
}

void TestWizardNavigator::markAllAttemptedMarksEveryStep()
{
    WizardNavigator nav(3);
    nav.setValid(0, true);
    nav.setValid(2, true);

    nav.markAllAttempted();
    for (int step = 0; step < nav.stepCount(); ++step)
        QVERIFY(nav.isAttempted(step));

    QVERIFY(nav.isComplete(0));
    QVERIFY(!nav.isComplete(1));
    QVERIFY(nav.isComplete(2));
    QCOMPARE(nav.visual(1), StepVisual::Error);
    // La palomita y el bloqueo son independientes: el 2 la lleva porque se
    // intentó y es válido, pero sigue bloqueado por el 1.
    QCOMPARE(nav.visual(2), StepVisual::Locked);
}

void TestWizardNavigator::allValidRequiresEveryStep()
{
    WizardNavigator nav(3);
    QVERIFY(!nav.allValid());

    nav.setValid(0, true);
    nav.setValid(1, true);
    QVERIFY(!nav.allValid());

    nav.setValid(2, true);
    QVERIFY(nav.allValid());

    nav.setValid(1, false);
    QVERIFY(!nav.allValid());
}

void TestWizardNavigator::outOfRangeIndicesAreIgnored()
{
    WizardNavigator nav(3);
    nav.setValid(0, true);

    // Los cambios fuera de rango no hacen nada...
    nav.setValid(-1, true);
    nav.setValid(3, true);
    nav.markAttempted(-1);
    nav.markAttempted(3);
    nav.moveTo(-1);
    nav.moveTo(3);
    QCOMPARE(nav.stepCount(), 3);
    QCOMPARE(nav.current(), 0);
    QVERIFY(!nav.isValid(1));
    QVERIFY(!nav.isValid(2));
    QVERIFY(!nav.isAttempted(0));

    // ...y las consultas contestan false, -1 o Locked en vez de tronar.
    QVERIFY(!nav.isValid(-1));
    QVERIFY(!nav.isValid(3));
    QVERIFY(!nav.isAttempted(-1));
    QVERIFY(!nav.isAttempted(3));
    QVERIFY(!nav.isComplete(-1));
    QVERIFY(!nav.isComplete(3));
    QVERIFY(!nav.canEnter(-1));
    QVERIFY(!nav.canEnter(3));
    QVERIFY(!nav.goTo(-1));
    QVERIFY(!nav.goTo(3));
    QCOMPARE(nav.current(), 0);
    QCOMPARE(nav.firstBlockingStep(-1), -1);
    QCOMPARE(nav.visual(-1), StepVisual::Locked);
    QCOMPARE(nav.visual(3), StepVisual::Locked);

    // Un conteo negativo deja un navegador vacío, no uno roto.
    WizardNavigator empty(-2);
    QCOMPARE(empty.stepCount(), 0);
    empty.setValid(0, true);
    empty.markAllAttempted();
    QVERIFY(!empty.isComplete(0));
    QVERIFY(!empty.canEnter(0));
    QVERIFY(!empty.goTo(0));
    QCOMPARE(empty.visual(0), StepVisual::Locked);
}

QTEST_APPLESS_MAIN(TestWizardNavigator)

#include "tst_wizardnavigator.moc"
