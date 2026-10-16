// Prueba de humo del expediente de un vehículo con sus widgets reales: el
// marco, las tres páginas del alta y el presenter, sobre puertos falsos.
// Cubre lo que las vistas falsas no ven: que bind() conecte de verdad, que el
// rol oculte lo que debe y que cada página siga en su área con scroll.

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "registrationfixtures.h"
#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"
#include "presentation/inventory/record/vehiclerecordpresenter.h"
#include "presentation/inventory/record/vehiclerecordview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QtTest>

#include <memory>

namespace {

template <class T>
T *fieldWidget(QWidget *root, const QString &field)
{
    for (T *widget : root->findChildren<T *>()) {
        if (widget->property("field").toString() == field)
            return widget;
    }
    return nullptr;
}

} // namespace

class TstVehicleRecordView : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryVehicleRepository m_vehicles;
    fakes::FakeFileStorage m_files;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeContractGenerator m_contracts;
    fakes::FakeVehicleRecordReader m_recordReader;
    fakes::InlineTaskRunner m_runner;
    std::unique_ptr<application::VehicleRegistrationService> m_forms;
    std::unique_ptr<application::VehicleRecordService> m_records;
    std::unique_ptr<VehicleRecordView> m_view;
    presentation::VehicleRecordPresenter *m_presenter = nullptr;

    void open(domain::UserRole role, int folio = 5)
    {
        m_view = std::make_unique<VehicleRecordView>();
        m_presenter = new presentation::VehicleRecordPresenter(
            *m_view, m_view->detailsView(), m_view->conditionsView(), m_view->filesView(), *m_forms,
            *m_records, m_runner, folio, role, m_view.get());
        m_view->bind(*m_presenter);
        m_presenter->start();
        m_view->show();
    }

    QStackedWidget *stack() const { return m_view->findChild<QStackedWidget *>(); }
    QPushButton *button(const char *name) const
    {
        return m_view->findChild<QPushButton *>(QString::fromLatin1(name));
    }

private slots:
    void init()
    {
        m_vehicles = fakes::InMemoryVehicleRepository();
        m_recordReader = fakes::FakeVehicleRecordReader();
        m_forms = std::make_unique<application::VehicleRegistrationService>(
            m_vehicles, m_files, m_reference, m_contracts);
        m_recordReader.records.insert(
            5, fixtures::storedRecord(5, domain::AcquisitionType::Adquisicion));
        m_records = std::make_unique<application::VehicleRecordService>(m_recordReader, m_vehicles,
                                                                       m_files, m_reference);
    }

    void cleanup()
    {
        m_view.reset();
        m_records.reset();
        m_forms.reset();
    }

    void opensOnDetailsWithItsFolio()
    {
        open(domain::UserRole::Admin, 5);
        QCOMPARE(stack()->currentIndex(), 0);
        QCOMPARE(m_view->folio(), 5);
        auto *title = m_view->findChild<QLabel *>(QStringLiteral("titleLabel"));
        QCOMPARE(title->text(), QStringLiteral("Folio 5 · Nissan Versa 2020"));
        // En consulta: Volver y, para el Administrador, Editar.
        QVERIFY(button("backButton")->isVisibleTo(m_view.get()));
        QVERIFY(button("editButton")->isVisibleTo(m_view.get()));
        QVERIFY(button("editButton")->isEnabled());
        QVERIFY(!button("saveButton")->isVisibleTo(m_view.get()));
        QVERIFY(!button("cancelEditButton")->isVisibleTo(m_view.get()));
    }

    void salesAgentCannotEdit()
    {
        open(domain::UserRole::SalesAgent);
        QVERIFY(!button("editButton")->isVisibleTo(m_view.get()));
        QVERIFY(button("backButton")->isVisibleTo(m_view.get()));
    }

    // US-03.4 con los widgets reales: Editar desbloquea los campos, salvo la
    // rama, el total de mantenimientos y Folio, y cambia los botones.
    void editUnlocksAllButTheLockedFields()
    {
        open(domain::UserRole::Admin);
        button("editButton")->click();
        QWidget *details = &m_view->detailsView();
        QVERIFY(!fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"))->isReadOnly());
        QVERIFY(!fieldWidget<QComboBox>(details, QStringLiteral("acquisitionType"))->isEnabled());
        QVERIFY(!fieldWidget<QDoubleSpinBox>(details, QStringLiteral("maintenanceCost"))->isEnabled());
        QVERIFY(!fieldWidget<QLineEdit>(details, QStringLiteral("folio"))->isEnabled());
        QVERIFY(fieldWidget<QComboBox>(details, QStringLiteral("brand"))->isEnabled());
        QWidget *files = &m_view->filesView();
        QVERIFY(files->findChild<QPushButton *>(QStringLiteral("addPhotoButton"))->isVisibleTo(files));

        QVERIFY(!button("backButton")->isVisibleTo(m_view.get()));
        QVERIFY(!button("editButton")->isVisibleTo(m_view.get()));
        QVERIFY(button("saveButton")->isVisibleTo(m_view.get()));
        QVERIFY(button("cancelEditButton")->isVisibleTo(m_view.get()));

        // Sin cambios, Cancelar no pregunta y todo vuelve a consulta.
        button("cancelEditButton")->click();
        QVERIFY(fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"))->isReadOnly());
        QVERIFY(fieldWidget<QComboBox>(details, QStringLiteral("acquisitionType"))->isEnabled());
        QVERIFY(button("editButton")->isVisibleTo(m_view.get()));
    }

    // Lo que se teclea en un campo real cuenta como cambio y se guarda; al
    // terminar, el expediente vuelve a consulta con su aviso.
    void typingAndSavingGoesThrough()
    {
        open(domain::UserRole::Admin);
        button("editButton")->click();
        QWidget *details = &m_view->detailsView();
        fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"))
            ->setText(QStringLiteral("3N1CN7AD0LL000002"));
        button("saveButton")->click();

        QCOMPARE(m_vehicles.updates, 1);
        QCOMPARE(m_vehicles.updatedSerialNumber, QStringLiteral("3N1CN7AD0LL000002"));
        QVERIFY(fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"))->isReadOnly());
        auto *notice = m_view->findChild<QLabel *>(QStringLiteral("noticeLabel"));
        QVERIFY(notice->isVisibleTo(m_view.get()));
        QCOMPARE(notice->text(), QStringLiteral("Cambios guardados."));
    }

    // Lo que se capture en el historial de mantenimientos no es una edición
    // de la unidad.
    void maintenanceHistoryIsNotAnEdit()
    {
        open(domain::UserRole::Admin);
        button("editButton")->click();
        QSignalSpy edited(&m_view->detailsView(), &VehicleDetailsView::edited);
        auto *table = m_view->detailsView().maintenanceHistory().findChild<QTableWidget *>();
        QVERIFY(table);
        table->setRowCount(1);
        table->setItem(0, 0, new QTableWidgetItem(QStringLiteral("01/03/2024")));
        QCOMPARE(edited.size(), 0);
        button("saveButton")->click();
        QCOMPARE(m_vehicles.updates, 0);
    }

    // Cada página sigue en su área con scroll, así cabe en una laptop chica.
    void everyStepScrolls()
    {
        open(domain::UserRole::Admin);
        const QList<QWidget *> pages{&m_view->detailsView(), &m_view->conditionsView(),
                                     &m_view->filesView()};
        for (QWidget *page : pages) {
            auto *area = qobject_cast<QScrollArea *>(page->parentWidget()->parentWidget());
            QVERIFY(area);
            QVERIFY(area->widgetResizable());
            QCOMPARE(area->widget(), page);
        }
    }

    void adminSeesTheDealCardAndTheHistory()
    {
        open(domain::UserRole::Admin);
        QWidget *page = &m_view->detailsView();
        QVERIFY(m_view->detailsView().maintenanceHistory().isVisibleTo(page));
        QVERIFY(fieldWidget<QLineEdit>(page, QStringLiteral("counterparty.fullName"))->isVisibleTo(page));
    }

    void salesAgentSeesNeitherTheDealCardNorTheHistory()
    {
        open(domain::UserRole::SalesAgent);
        QWidget *page = &m_view->detailsView();
        QVERIFY(!m_view->detailsView().maintenanceHistory().isVisibleTo(page));
        QVERIFY(!fieldWidget<QLineEdit>(page, QStringLiteral("counterparty.fullName"))->isVisibleTo(page));
        // La tarjeta del vehículo sí se ve.
        QVERIFY(fieldWidget<QLineEdit>(page, QStringLiteral("serialNumber"))->isVisibleTo(page));
    }

    // #30 con los widgets reales: cada página muestra lo guardado, el folio
    // incluido, y "Ver" abre los documentos.
    void paintsTheStoredVehicle()
    {
        open(domain::UserRole::Admin, 5);
        const application::VehicleRecordDto stored = m_recordReader.records.value(5);
        QWidget *details = &m_view->detailsView();
        QCOMPARE(fieldWidget<QLineEdit>(details, QStringLiteral("folio"))->text(),
                 QStringLiteral("5"));
        QCOMPARE(fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"))->text(),
                 stored.details.serialNumber);
        QCOMPARE(fieldWidget<QComboBox>(details, QStringLiteral("brand"))->currentText(),
                 QStringLiteral("Nissan"));
        QCOMPARE(fieldWidget<QLineEdit>(details, QStringLiteral("counterparty.fullName"))->text(),
                 stored.details.counterparty.fullName);
        QCOMPARE(details->findChild<QLabel *>(QStringLiteral("invoiceFileLabel"))->text(),
                 QStringLiteral("factura.pdf"));
        // En consulta, la factura se abre con "Ver"; subirla, no.
        QVERIFY(details->findChild<QPushButton *>(QStringLiteral("invoiceViewButton"))
                    ->isVisibleTo(details));
        QVERIFY(!details->findChild<QPushButton *>(QStringLiteral("invoiceUploadButton"))
                     ->isVisibleTo(details));

        const application::VehicleConditionsDto conditions = m_view->conditionsView().conditions();
        QCOMPARE(conditions.transmission, stored.conditions.transmission);
        QCOMPARE(conditions.inspection.size(), 1);
        QVERIFY(!conditions.inspection.first().isOptimal);

        const application::VehicleFilesDto files = m_view->filesView().files();
        QCOMPARE(files.images.size(), 2);
        QVERIFY(files.images.at(1).isPrimary);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QWidget *filesPage = &m_view->filesView();
        QVERIFY(filesPage->findChild<QPushButton *>(QStringLiteral("documentViewButton")));
    }

    // Con los widgets reales: los campos no se editan y la captura de
    // archivos no se ofrece.
    void opensReadOnly()
    {
        open(domain::UserRole::Admin);
        QWidget *details = &m_view->detailsView();
        QVERIFY(fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"))->isReadOnly());
        QWidget *files = &m_view->filesView();
        QVERIFY(!files->findChild<QPushButton *>(QStringLiteral("addPhotoButton"))->isVisibleTo(files));
        QVERIFY(!files->acceptDrops());
    }

    void tabsSwitchPages()
    {
        open(domain::UserRole::SalesAgent);
        m_presenter->onStepClicked(2);
        QCOMPARE(stack()->currentIndex(), 2);
        m_presenter->onStepClicked(0);
        QCOMPARE(stack()->currentIndex(), 0);
    }

    void backButtonReturnsToTheInventory()
    {
        open(domain::UserRole::Admin);
        QSignalSpy back(m_view.get(), &VehicleRecordView::returnToInventory);
        button("backButton")->click();
        QCOMPARE(back.size(), 1);
    }
};

QTEST_MAIN(TstVehicleRecordView)
#include "tst_vehiclerecordview.moc"
