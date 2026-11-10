// Pruebas del presenter del historial de mantenimientos: pinta el historial
// del folio, "+ Agregar Mantenimiento" abre el formulario con el catálogo, y
// Aceptar guarda, cierra, repinta y avisa el total nuevo (#46), o deja el
// formulario abierto con los errores.

#include "application/inventory/maintenance/services/maintenanceservice.h"
#include "fakes.h"
#include "presentation/inventory/maintenance/maintenancehistorypresenter.h"

#include <QSignalSpy>
#include <QtTest>

using domain::UserRole;

namespace {

constexpr int kFolio = 7;

application::NewMaintenanceDto captured(const QString &cost = QStringLiteral("1500.50"))
{
    // Sin folio: lo pone el presenter.
    application::NewMaintenanceDto dto;
    dto.date = QDate(2026, 10, 19);
    dto.maintenanceTypeId = 1;
    dto.cost = cost;
    dto.description = QStringLiteral("Cambio de aceite");
    return dto;
}

} // namespace

class TstMaintenanceHistoryPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryMaintenanceRepository m_maintenances;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeMaintenanceReceiptGenerator m_receipts;
    fakes::FakeMaintenanceHistoryView m_view;

    application::MaintenanceService service() { return {m_maintenances, m_reference, m_receipts}; }

private slots:
    void init()
    {
        m_maintenances = fakes::InMemoryMaintenanceRepository();
        m_reference = fakes::FakeReferenceDataReader();
        m_receipts = fakes::FakeMaintenanceReceiptGenerator();
        m_view = fakes::FakeMaintenanceHistoryView();
    }

    // --- Historial ---

    void startPaintsTheHistoryOfItsFolio()
    {
        application::MaintenanceItemDto item;
        item.id = 3;
        m_maintenances.byFolio.insert(kFolio, {item});
        m_maintenances.byFolio.insert(99, {application::MaintenanceItemDto()});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.start();

        QCOMPARE(m_view.shownHistories.size(), 1);
        QCOMPARE(m_view.shownHistories.first().size(), 1);
        QCOMPARE(m_view.shownHistories.first().first().id, 3);
    }

    void aSellerGetsNothingPaintedNorWarned()
    {
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::SalesAgent);

        presenter.start();

        QVERIFY(m_view.shownHistories.isEmpty());
        QVERIFY(m_view.warnings.isEmpty());
    }

    void aFailedHistoryWarns()
    {
        m_maintenances.failWith = QStringLiteral("sin conexión");
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.start();

        QVERIFY(m_view.shownHistories.isEmpty());
        QCOMPARE(m_view.warnings.size(), 1);
    }

    // --- Abrir el formulario ---

    void addOpensTheFormWithTheCatalog()
    {
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.onAddMaintenance();

        QCOMPARE(m_view.openedForms.size(), 1);
        QCOMPARE(m_view.openedForms.first().size(), 2);
        QCOMPARE(m_view.openedForms.first().at(1).name, QStringLiteral("Llantas"));
        QCOMPARE(m_view.busyChanges, QList<bool>({true, false}));
        QVERIFY(m_view.warnings.isEmpty());
    }

    void aFailedReadWarnsInsteadOfOpeningAnEmptyForm()
    {
        m_reference.maintenanceTypesError = QStringLiteral("sin conexión");
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.onAddMaintenance();

        QVERIFY(m_view.openedForms.isEmpty());
        QCOMPARE(m_view.warnings.size(), 1);
        QCOMPARE(m_view.busyChanges, QList<bool>({true, false}));
    }

    void aSellerNeverGetsTheForm()
    {
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::SalesAgent);

        presenter.onAddMaintenance();

        QVERIFY(m_view.openedForms.isEmpty());
        QCOMPARE(m_view.warnings.size(), 1);
    }

    void aSecondClickWhileReadingDoesNotOpenTwoForms()
    {
        const application::MaintenanceService svc = service();
        fakes::DeferredTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.onAddMaintenance();
        presenter.onAddMaintenance();
        QCOMPARE(runner.works.size(), 1);

        runner.finish(0);
        QCOMPARE(m_view.openedForms.size(), 1);

        // Ya cerrado el primero, otro clic sí vuelve a leer.
        presenter.onAddMaintenance();
        QCOMPARE(runner.works.size(), 2);
    }

    // --- Guardar (#46) ---

    void acceptSavesWithTheRecordFolioClosesAndRepaints()
    {
        m_maintenances.totals.insert(kFolio, 500.0);
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        QSignalSpy totals(&presenter, &presentation::MaintenanceHistoryPresenter::maintenanceCostChanged);

        presenter.onSubmitMaintenance(captured());

        QCOMPARE(m_maintenances.added.size(), 1);
        QCOMPARE(m_maintenances.added.first().vehicleFolio(), kFolio);
        QCOMPARE(m_view.formsClosed, 1);
        QCOMPARE(m_view.formBusyChanges, QList<bool>({true, false}));
        // La tabla se vuelve a leer y ya trae el renglón nuevo.
        QCOMPARE(m_view.shownHistories.size(), 1);
        QCOMPARE(m_view.shownHistories.first().first().id, 100);
        // Nace sin recibo impreso.
        QVERIFY(!m_view.shownHistories.first().first().receiptPrinted);
        QVERIFY(m_view.checkedReceipts.isEmpty());
        QCOMPARE(totals.size(), 1);
        QCOMPARE(totals.first().first().toDouble(), 2000.50);
    }

    void withoutDealRowThereIsNoTotalToRepaint()
    {
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        QSignalSpy totals(&presenter, &presentation::MaintenanceHistoryPresenter::maintenanceCostChanged);

        presenter.onSubmitMaintenance(captured());

        QCOMPARE(m_view.formsClosed, 1);
        QVERIFY(totals.isEmpty());
    }

    void errorsStayInTheOpenForm()
    {
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.onSubmitMaintenance(captured(QStringLiteral("mil")));

        QCOMPARE(m_view.formsClosed, 0);
        QCOMPARE(m_view.formErrors.size(), 1);
        QCOMPARE(m_view.formErrors.first().first().field, QStringLiteral("cost"));
        QVERIFY(m_maintenances.added.isEmpty());
        QVERIFY(m_view.shownHistories.isEmpty());
    }

    void aFailedSaveWarnsAndKeepsTheForm()
    {
        m_maintenances.failWith = QStringLiteral("sin conexión");
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.onSubmitMaintenance(captured());

        QCOMPARE(m_view.formsClosed, 0);
        QCOMPARE(m_view.warnings.size(), 1);
        QCOMPARE(m_view.formBusyChanges, QList<bool>({true, false}));
    }

    void aSecondAcceptWhileSavingIsIgnored()
    {
        const application::MaintenanceService svc = service();
        fakes::DeferredTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);

        presenter.onSubmitMaintenance(captured());
        presenter.onSubmitMaintenance(captured());
        QCOMPARE(runner.works.size(), 1);
    }

    // --- Recibo (#47) ---

    void printMakesTheReceiptOfTheSelectedRowAndOpensIt()
    {
        application::MaintenanceItemDto older;
        older.id = 3;
        application::MaintenanceItemDto selected;
        selected.id = 4;
        selected.typeName = QStringLiteral("Llantas");
        selected.cost = 1500.5;
        m_maintenances.byFolio.insert(kFolio, {selected, older});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.selectedId = 4;

        presenter.onPrintReceipt();

        // Los datos tal como se leyeron, no el texto de la tabla.
        QCOMPARE(m_receipts.generated.size(), 1);
        QCOMPARE(m_receipts.generated.first().typeName, QStringLiteral("Llantas"));
        QCOMPARE(m_receipts.generated.first().cost, 1500.5);
        QCOMPARE(m_view.suggestedReceiptNames,
                 QStringList{QStringLiteral("recibo_mantenimiento_folio7_4.pdf")});
        QCOMPARE(m_view.openedDocuments, QStringList{m_view.receiptPath});
        QVERIFY(m_view.warnings.isEmpty());
        // La primera vez queda anotado en la base y se marca su casilla.
        QCOMPARE(m_maintenances.markedPrinted, QList<int>{4});
        QCOMPARE(m_view.checkedReceipts, QList<int>{4});
    }

    void reprintingNeedsNothingMoreToBeNoted()
    {
        application::MaintenanceItemDto printed;
        printed.id = 4;
        printed.receiptPrinted = true;
        m_maintenances.byFolio.insert(kFolio, {printed});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.selectedId = 4;

        presenter.onPrintReceipt();
        presenter.onPrintReceipt();

        // Se imprime cada vez, pero ya no hay nada que anotar.
        QCOMPARE(m_receipts.generated.size(), 2);
        QVERIFY(m_maintenances.markedPrinted.isEmpty());
    }

    void theSecondPrintOfANewReceiptDoesNotNoteItAgain()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.selectedId = 4;

        presenter.onPrintReceipt();
        presenter.onPrintReceipt();

        QCOMPARE(m_receipts.generated.size(), 2);
        QCOMPARE(m_maintenances.markedPrinted, QList<int>{4});
    }

    // --- Corregir la descripción en la tabla ---

    void acceptingSavesTheNewDescriptionAndSaysSo()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        item.description = QStringLiteral("Afinación");
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();

        presenter.onDescriptionEdited(4, QStringLiteral("  Afinación mayor  "));

        QCOMPARE(m_view.descriptionConfirmations, 1);
        QCOMPARE(m_maintenances.byFolio.value(kFolio).first().description,
                 QStringLiteral("Afinación mayor"));
        QCOMPARE(m_view.informations,
                 QStringList{QStringLiteral("El registro se actualizó con éxito")});
        // Se repinta con lo que quedó guardado.
        QCOMPARE(m_view.shownHistories.last().first().description,
                 QStringLiteral("Afinación mayor"));
    }

    void cancellingRestoresTheTableAndSaysSo()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        item.description = QStringLiteral("Afinación");
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.acceptDescriptionUpdate = false;

        presenter.onDescriptionEdited(4, QStringLiteral("Otra cosa"));

        QCOMPARE(m_maintenances.byFolio.value(kFolio).first().description,
                 QStringLiteral("Afinación"));
        QCOMPARE(m_view.informations, QStringList{QStringLiteral("Actualización cancelada")});
        QCOMPARE(m_view.shownHistories.size(), 2);
        QCOMPARE(m_view.shownHistories.last().first().description, QStringLiteral("Afinación"));
    }

    void anUnchangedDescriptionAsksNothing()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        item.description = QStringLiteral("Afinación");
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();

        presenter.onDescriptionEdited(4, QStringLiteral("Afinación "));

        QCOMPARE(m_view.descriptionConfirmations, 0);
        QVERIFY(m_view.informations.isEmpty());
    }

    void anEmptyDescriptionIsNotSavedAndTheTableGoesBack()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        item.description = QStringLiteral("Afinación");
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();

        presenter.onDescriptionEdited(4, QStringLiteral("   "));

        QCOMPARE(m_maintenances.byFolio.value(kFolio).first().description,
                 QStringLiteral("Afinación"));
        QCOMPARE(m_view.warnings.size(), 1);
        QVERIFY(m_view.informations.isEmpty());
        QCOMPARE(m_view.shownHistories.last().first().description, QStringLiteral("Afinación"));
    }

    void aFailedUpdateWarnsAndTheTableGoesBack()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        item.description = QStringLiteral("Afinación");
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_maintenances.failWith = QStringLiteral("sin conexión");

        presenter.onDescriptionEdited(4, QStringLiteral("Otra cosa"));

        QCOMPARE(m_view.warnings.size(), 1);
        QVERIFY(m_view.informations.isEmpty());
        QCOMPARE(m_view.shownHistories.last().first().description, QStringLiteral("Afinación"));
    }

    void aFailedNoteWarnsButTheReceiptWasMade()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.selectedId = 4;
        m_maintenances.failWith = QStringLiteral("sin conexión");

        presenter.onPrintReceipt();

        QCOMPARE(m_view.openedDocuments.size(), 1);
        QVERIFY(m_view.checkedReceipts.isEmpty());
        QCOMPARE(m_view.warnings.size(), 1);
    }

    void printWithoutSelectionAsksToChooseOne()
    {
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();

        presenter.onPrintReceipt();

        QCOMPARE(m_view.warnings.size(), 1);
        QVERIFY(m_view.suggestedReceiptNames.isEmpty());
        QVERIFY(m_receipts.generated.isEmpty());
    }

    void cancellingTheSaveDialogPrintsNothing()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        m_maintenances.byFolio.insert(kFolio, {item});
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.selectedId = 4;
        m_view.receiptPath.clear();

        presenter.onPrintReceipt();

        QVERIFY(m_receipts.generated.isEmpty());
        QVERIFY(m_view.openedDocuments.isEmpty());
        QVERIFY(m_view.warnings.isEmpty());
    }

    void aFailedReceiptWarnsAndOpensNothing()
    {
        application::MaintenanceItemDto item;
        item.id = 4;
        m_maintenances.byFolio.insert(kFolio, {item});
        m_receipts.failWith = QStringLiteral("plantilla incompleta");
        const application::MaintenanceService svc = service();
        fakes::InlineTaskRunner runner;
        presentation::MaintenanceHistoryPresenter presenter(m_view, svc, runner, kFolio,
                                                            UserRole::Admin);
        presenter.start();
        m_view.selectedId = 4;

        presenter.onPrintReceipt();

        QCOMPARE(m_view.warnings.size(), 1);
        QVERIFY(m_view.openedDocuments.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstMaintenanceHistoryPresenter)
#include "tst_maintenancehistorypresenter.moc"
