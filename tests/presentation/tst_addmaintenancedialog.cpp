// El historial de mantenimientos con widgets reales: el formulario de alta
// (sus campos, la fecha de hoy, el catálogo en el combo, lo que entrega
// Aceptar y los errores bajo cada campo), la tabla, y el camino completo con
// su presenter: abrir, guardar, cerrar y repintar.

#include "application/inventory/maintenance/services/maintenanceservice.h"
#include "fakes.h"
#include "presentation/inventory/maintenance/addmaintenancedialog.h"
#include "presentation/inventory/maintenance/maintenancehistorypresenter.h"
#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTextEdit>
#include <QtTest>

namespace {

QList<application::CatalogOptionDto> types()
{
    return {{1, QStringLiteral("Mecánica"), -1}, {5, QStringLiteral("Llantas"), -1}};
}

QPushButton *button(QWidget *root, const QString &text)
{
    for (QPushButton *candidate : root->findChildren<QPushButton *>()) {
        if (candidate->text() == text)
            return candidate;
    }
    return nullptr;
}

// Llena el formulario como lo haría el usuario.
void fill(AddMaintenanceDialog &dialog, const QString &cost)
{
    dialog.findChild<QComboBox *>()->setCurrentIndex(1); // Llantas
    dialog.findChild<QLineEdit *>(QStringLiteral("maintenanceCost"))->setText(cost);
    dialog.findChild<QTextEdit *>()->setPlainText(QStringLiteral("Cuatro llantas nuevas"));
}

application::MaintenanceItemDto row(int id, const QString &type, double cost,
                                    bool receiptPrinted = false)
{
    application::MaintenanceItemDto item;
    item.id = id;
    item.date = QDate(2026, 10, 19);
    item.typeName = type;
    item.cost = cost;
    item.description = QStringLiteral("Detalle");
    item.receiptPrinted = receiptPrinted;
    return item;
}

} // namespace

class TstAddMaintenanceDialog : public QObject
{
    Q_OBJECT

private slots:
    // --- El formulario ---

    void hasTheRequestedFields()
    {
        AddMaintenanceDialog dialog(types());

        QStringList labels;
        for (const QLabel *label : dialog.findChildren<QLabel *>())
            labels << label->text();
        for (const QString &expected : {QStringLiteral("Fecha:"),
                                        QStringLiteral("Tipo de Mantenimiento:"),
                                        QStringLiteral("Descripción:")})
            QVERIFY2(labels.contains(expected), qPrintable(expected));
        // Recibo ya no se captura: lo marca "Imprimir" en la tabla.
        QVERIFY(!labels.contains(QStringLiteral("Recibo:")));

        QVERIFY(dialog.findChild<QLineEdit *>(QStringLiteral("maintenanceCost")));
        QCOMPARE(dialog.findChildren<QTextEdit *>().size(), 1);
        QVERIFY(dialog.findChildren<QCheckBox *>().isEmpty());
        QVERIFY(dialog.isModal());
    }

    void startsOnTodayWithTheCatalog()
    {
        AddMaintenanceDialog dialog(types());

        QCOMPARE(dialog.findChild<QDateEdit *>()->date(), QDate::currentDate());
        const QComboBox *combo = dialog.findChild<QComboBox *>();
        // Los dos tipos, sin elegir: "Seleccione una opción..." es el texto
        // de ayuda del combo, no un renglón.
        QCOMPARE(combo->count(), 2);
        QCOMPARE(combo->itemText(0), QStringLiteral("Mecánica"));
        QCOMPARE(combo->itemText(1), QStringLiteral("Llantas"));
        QCOMPARE(combo->currentIndex(), -1);
    }

    void acceptHandsOverWhatWasCapturedWithoutClosing()
    {
        AddMaintenanceDialog dialog(types());
        dialog.show();
        dialog.findChild<QDateEdit *>()->setDate(QDate(2026, 10, 1));
        fill(dialog, QStringLiteral("2500.75"));
        QSignalSpy submitted(&dialog, &AddMaintenanceDialog::submitted);

        button(&dialog, QStringLiteral("Aceptar"))->click();

        QCOMPARE(submitted.size(), 1);
        const auto dto = submitted.first().at(0).value<application::NewMaintenanceDto>();
        QCOMPARE(dto.date, QDate(2026, 10, 1));
        QCOMPARE(dto.maintenanceTypeId, std::optional(5));
        QCOMPARE(dto.cost, QStringLiteral("2500.75"));
        QCOMPARE(dto.description, QStringLiteral("Cuatro llantas nuevas"));
        // Cerrar lo decide quien guarda.
        QVERIFY(dialog.isVisible());
    }

    void noTypeChosenGoesAsUnchosen()
    {
        AddMaintenanceDialog dialog(types());
        QVERIFY(!dialog.maintenance().maintenanceTypeId);
    }

    void cancelClosesIt()
    {
        AddMaintenanceDialog dialog(types());
        dialog.show();
        button(&dialog, QStringLiteral("Cancelar"))->click();
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }

    void cancelSitsRightOfAccept()
    {
        AddMaintenanceDialog dialog(types());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));

        const QPushButton *accept = button(&dialog, QStringLiteral("Aceptar"));
        const QPushButton *cancel = button(&dialog, QStringLiteral("Cancelar"));
        QVERIFY(cancel->geometry().left() > accept->geometry().right());
        // Alineados a la derecha: el espacio libre queda a su izquierda.
        QVERIFY(accept->geometry().left() > dialog.width() / 2);
    }

    void errorsAppearUnderTheirField()
    {
        AddMaintenanceDialog dialog(types());
        dialog.show();
        auto *cost = dialog.findChild<QLineEdit *>(QStringLiteral("maintenanceCost"));

        dialog.showErrors({{QStringLiteral("cost"), QStringLiteral("El costo admite hasta 2 decimales.")}});

        QCOMPARE(cost->property("hasError").toBool(), true);
        QStringList shown;
        for (const QLabel *label : dialog.findChildren<QLabel *>()) {
            if (label->isVisible())
                shown << label->text();
        }
        QVERIFY(shown.contains(QStringLiteral("El costo admite hasta 2 decimales.")));
        // Debajo del campo, no en su tooltip.
        QVERIFY(cost->toolTip().isEmpty());
    }

    // --- La tabla ---

    void theTablePaintsEachMaintenanceWithItsPrintedReceipt()
    {
        MaintenanceHistoryTableWidget history;
        auto *table = history.findChild<QTableWidget *>(QStringLiteral("historyTable"));
        using Column = MaintenanceHistoryTableWidget::Column;

        history.showMaintenances({row(2, QStringLiteral("Llantas"), 1500.5),
                                  row(1, QStringLiteral("Mecánica"), 300, true)});
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, Column::DateColumn)->text(), QStringLiteral("19/10/2026"));
        QCOMPARE(table->item(0, Column::TypeColumn)->text(), QStringLiteral("Llantas"));
        QCOMPARE(table->item(0, Column::CostColumn)->text(), QStringLiteral("$1,500.50"));
        QCOMPARE(table->item(0, Column::DescriptionColumn)->text(), QStringLiteral("Detalle"));
        QCOMPARE(table->item(0, Column::ReceiptColumn)->checkState(), Qt::Unchecked);
        QCOMPARE(table->item(1, Column::ReceiptColumn)->checkState(), Qt::Checked);

        // Solo informa: el usuario no la puede cambiar.
        QVERIFY(!(table->item(0, Column::ReceiptColumn)->flags() & Qt::ItemIsUserCheckable));

        history.checkReceipt(2);
        QCOMPARE(table->item(0, Column::ReceiptColumn)->checkState(), Qt::Checked);
    }

    void onlyTheDescriptionIsEditedByDoubleClick()
    {
        MaintenanceHistoryTableWidget history;
        auto *table = history.findChild<QTableWidget *>(QStringLiteral("historyTable"));
        using Column = MaintenanceHistoryTableWidget::Column;
        history.showMaintenances({row(2, QStringLiteral("Llantas"), 1500.5)});

        QCOMPARE(table->editTriggers(), QAbstractItemView::DoubleClicked);
        for (int column : {Column::DateColumn, Column::TypeColumn, Column::CostColumn,
                           Column::ReceiptColumn})
            QVERIFY(!(table->item(0, column)->flags() & Qt::ItemIsEditable));
        QVERIFY(table->item(0, Column::DescriptionColumn)->flags() & Qt::ItemIsEditable);
    }

    void editingTheDescriptionAnnouncesItOnce()
    {
        MaintenanceHistoryTableWidget history;
        auto *table = history.findChild<QTableWidget *>(QStringLiteral("historyTable"));
        using Column = MaintenanceHistoryTableWidget::Column;
        QSignalSpy edited(&history, &MaintenanceHistoryTableWidget::descriptionEdited);

        // Lo que pinta la vista (y marcar un recibo) no es una edición.
        history.showMaintenances({row(2, QStringLiteral("Llantas"), 1500.5)});
        history.checkReceipt(2);
        QCoreApplication::processEvents();
        QVERIFY(edited.isEmpty());

        // Lo que confirma el editor de la celda llega como un cambio del item.
        table->item(0, Column::DescriptionColumn)->setText(QStringLiteral("Balanceo"));
        QTRY_COMPARE(edited.size(), 1);
        QCOMPARE(edited.first().at(0).toInt(), 2);
        QCOMPARE(edited.first().at(1).toString(), QStringLiteral("Balanceo"));
    }

    void theSelectedRowGivesItsMaintenance()
    {
        MaintenanceHistoryTableWidget history;
        auto *table = history.findChild<QTableWidget *>(QStringLiteral("historyTable"));
        history.showMaintenances({row(2, QStringLiteral("Llantas"), 1500.5),
                                  row(1, QStringLiteral("Mecánica"), 300)});

        QCOMPARE(history.selectedMaintenanceId(), -1);
        table->selectRow(1);
        QCOMPARE(history.selectedMaintenanceId(), 1);
        // Un renglón a la vez: el recibo es de uno.
        QCOMPARE(table->selectionMode(), QAbstractItemView::SingleSelection);
    }

    void printAnnouncesItself()
    {
        MaintenanceHistoryTableWidget history;
        QSignalSpy printed(&history, &MaintenanceHistoryTableWidget::printReceiptsRequested);
        history.findChild<QPushButton *>(QStringLiteral("printReceiptsButton"))->click();
        QCOMPARE(printed.size(), 1);
    }

    // El formulario no cuelga del Paso 1: comparte claves de campo con él
    // ("description"), y el Paso 1 busca sus campos entre sus descendientes.
    void theFormLivesOutsideTheSection()
    {
        QWidget window;
        auto *history = new MaintenanceHistoryTableWidget(&window);
        history->askNewMaintenance(types());

        QVERIFY(window.findChild<AddMaintenanceDialog *>());
        QVERIFY(!history->findChild<AddMaintenanceDialog *>());
    }

    // --- Con su presenter ---

    void addingAMaintenanceEndToEnd()
    {
        fakes::InMemoryMaintenanceRepository maintenances;
        maintenances.totals.insert(7, 0.0);
        fakes::FakeReferenceDataReader reference;
        fakes::FakeMaintenanceReceiptGenerator receipts;
        const application::MaintenanceService service(maintenances, reference, receipts);
        fakes::InlineTaskRunner runner;
        MaintenanceHistoryTableWidget history;
        presentation::MaintenanceHistoryPresenter presenter(history, service, runner, 7,
                                                            domain::UserRole::Admin);
        history.bind(presenter);
        QSignalSpy totals(&presenter,
                          &presentation::MaintenanceHistoryPresenter::maintenanceCostChanged);
        auto *table = history.findChild<QTableWidget *>(QStringLiteral("historyTable"));

        emit history.addMaintenanceRequested();
        auto *dialog = history.findChild<AddMaintenanceDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());

        // Primero con un costo que no cabe: el formulario sigue abierto.
        fill(*dialog, QStringLiteral("10.505"));
        button(dialog, QStringLiteral("Aceptar"))->click();
        QVERIFY(dialog->isVisible());
        QVERIFY(maintenances.added.isEmpty());

        fill(*dialog, QStringLiteral("10.50"));
        button(dialog, QStringLiteral("Aceptar"))->click();
        QTRY_VERIFY(!history.findChild<AddMaintenanceDialog *>());

        QCOMPARE(maintenances.added.size(), 1);
        QCOMPARE(maintenances.added.first().vehicleFolio(), 7);
        QCOMPARE(table->rowCount(), 1);
        // Nace sin recibo impreso.
        QCOMPARE(table->item(0, MaintenanceHistoryTableWidget::ReceiptColumn)->checkState(),
                 Qt::Unchecked);
        QCOMPARE(totals.size(), 1);
    }
};

QTEST_MAIN(TstAddMaintenanceDialog)
#include "tst_addmaintenancedialog.moc"
