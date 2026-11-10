// Modo de un formulario que se consulta y se corrige (el expediente, #33):
// consulta o edición, y si hay cambios sin guardar.

#include "presentation/common/navigation/editsession.h"

#include <QtTest>

using presentation::EditSession;
using presentation::FormMode;

class TstEditSession : public QObject
{
    Q_OBJECT

private slots:
    void startsViewingWithNothingToLose()
    {
        const EditSession session;
        QCOMPARE(session.mode(), FormMode::View);
        QVERIFY(!session.isEditing());
        QVERIFY(!session.canEdit());
        QVERIFY(!session.isDirty());
        QVERIFY(!session.needsConfirmationToLeave());
    }

    // Sin permiso no se edita; con él, se entra limpio.
    void editingNeedsPermission()
    {
        EditSession session;
        QVERIFY(!session.beginEdit());
        QCOMPARE(session.mode(), FormMode::View);

        session.allowEditing(true);
        QVERIFY(session.beginEdit());
        QCOMPARE(session.mode(), FormMode::Edit);
        QVERIFY(!session.isDirty());
        // Entrar sin cambiar nada no pide confirmación al salir.
        QVERIFY(!session.needsConfirmationToLeave());
    }

    // Lo que los campos avisan en consulta (al pintar lo guardado) no es un
    // cambio del usuario.
    void editsOnlyCountWhileEditing()
    {
        EditSession session;
        session.allowEditing(true);
        session.markEdited();
        QVERIFY(!session.isDirty());

        session.beginEdit();
        session.markEdited();
        QVERIFY(session.isDirty());
        QVERIFY(session.needsConfirmationToLeave());
    }

    // Pedir editar otra vez a media edición no borra lo capturado.
    void beginningAgainKeepsTheChanges()
    {
        EditSession session;
        session.allowEditing(true);
        session.beginEdit();
        session.markEdited();
        QVERIFY(!session.beginEdit());
        QVERIFY(session.isDirty());
    }

    // Guardar o descartar vuelve a consulta, sin nada pendiente.
    void endingReturnsToViewClean()
    {
        EditSession session;
        session.allowEditing(true);
        session.beginEdit();
        session.markEdited();
        session.endEdit();
        QCOMPARE(session.mode(), FormMode::View);
        QVERIFY(!session.isDirty());
        QVERIFY(!session.needsConfirmationToLeave());
        // Y se puede volver a editar.
        QVERIFY(session.beginEdit());
    }

    // Quitar el permiso no saca de la edición: qué hacer con lo capturado lo
    // decide quien la usa. Sí impide volver a entrar.
    void revokingPermissionDoesNotDropChanges()
    {
        EditSession session;
        session.allowEditing(true);
        session.beginEdit();
        session.markEdited();
        session.allowEditing(false);
        QVERIFY(session.isEditing());
        QVERIFY(session.isDirty());

        session.endEdit();
        QVERIFY(!session.beginEdit());
    }
};

QTEST_APPLESS_MAIN(TstEditSession)
#include "tst_editsession.moc"
