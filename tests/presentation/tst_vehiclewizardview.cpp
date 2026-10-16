// Prueba de humo del asistente con sus widgets reales: la vista, sus tres
// páginas y el presenter, sobre puertos falsos. Cubre lo que las vistas falsas
// no ven: que bind() conecte de verdad, que las páginas pinten lo que el
// presenter manda y que nada truene al recorrer los pasos.

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "registrationfixtures.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"
#include "presentation/inventory/registration/vehiclewizardpresenter.h"
#include "presentation/common/components/aspectratioimagelabel.h"
#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"
#include "presentation/inventory/registration/vehiclewizardview.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
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

QPushButton *buttonWithText(QWidget *root, const QString &text)
{
    for (QPushButton *button : root->findChildren<QPushButton *>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

// Los layouts se recalculan con eventos encolados, uno por nivel de
// anidación (la sección de una rama está dentro de la tarjeta, y esta dentro
// del paso).
void settleLayouts()
{
    for (int i = 0; i < 5; ++i)
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}

// La casilla de la fila de un tipo de documento.
QCheckBox *documentCheck(QWidget *files, const QString &type)
{
    for (QWidget *row : files->findChildren<QWidget *>(QStringLiteral("documentRow"))) {
        if (row->property("documentType").toString() == type)
            return row->findChild<QCheckBox *>(QStringLiteral("documentCheck"));
    }
    return nullptr;
}

// Una unidad guardada con todos los campos del Paso 1 llenos, de las dos
// ramas (cada una con sus términos).
application::VehicleDetailsDto completeDetails(domain::AcquisitionType type)
{
    application::VehicleDetailsDto details = fixtures::validRegistration(type).details;
    details.dealDate = QDate(2024, 3, 15);
    details.color = QStringLiteral("Rojo");
    details.mileage = 45000;
    details.description = QStringLiteral("Un solo dueño");
    details.counterparty.streetAddress = QStringLiteral("Calle 1 #23");
    details.counterparty.suburb = QStringLiteral("Centro");
    details.counterparty.locality = QStringLiteral("Morelia");
    details.counterparty.state = QStringLiteral("Michoacán");
    details.counterparty.postalCode = QStringLiteral("58000");
    details.invoiceIssuer = QStringLiteral("Agencia del Centro");
    details.maintenanceCost = 1500.5;
    details.observations = QStringLiteral("Sin detalles");
    if (type == domain::AcquisitionType::Adquisicion) {
        details.paymentType = domain::PaymentType::Credito;
        details.paymentMethod = domain::PaymentMethod::Efectivo;
    } else {
        details.commissionRate = 12.5;
    }
    return details;
}

// Campo por campo, salvo la factura adjunta, que no viaja en details().
void compareDetails(const application::VehicleDetailsDto &actual,
                    const application::VehicleDetailsDto &expected)
{
    QCOMPARE(actual.acquisitionType, expected.acquisitionType);
    QCOMPARE(actual.dealDate, expected.dealDate);
    QCOMPARE(actual.vehicleType.id, expected.vehicleType.id);
    QCOMPARE(actual.subtype.id, expected.subtype.id);
    QCOMPARE(actual.brand.id, expected.brand.id);
    QCOMPARE(actual.brand.name, expected.brand.name);
    QCOMPARE(actual.model, expected.model);
    QCOMPARE(actual.yearModel, expected.yearModel);
    QCOMPARE(actual.color, expected.color);
    QCOMPARE(actual.mileage, expected.mileage);
    QCOMPARE(actual.description, expected.description);
    QCOMPARE(actual.motorNumber, expected.motorNumber);
    QCOMPARE(actual.serialNumber, expected.serialNumber);
    QCOMPARE(actual.repuve, expected.repuve);
    QCOMPARE(actual.plates, expected.plates);
    QCOMPARE(actual.platesHolder, expected.platesHolder);
    QCOMPARE(actual.counterparty.fullName, expected.counterparty.fullName);
    QCOMPARE(actual.counterparty.nationalId, expected.counterparty.nationalId);
    QCOMPARE(actual.counterparty.streetAddress, expected.counterparty.streetAddress);
    QCOMPARE(actual.counterparty.suburb, expected.counterparty.suburb);
    QCOMPARE(actual.counterparty.locality, expected.counterparty.locality);
    QCOMPARE(actual.counterparty.state, expected.counterparty.state);
    QCOMPARE(actual.counterparty.postalCode, expected.counterparty.postalCode);
    QCOMPARE(actual.invoiceType, expected.invoiceType);
    QCOMPARE(actual.invoiceNumber, expected.invoiceNumber);
    QCOMPARE(actual.invoiceIssuer, expected.invoiceIssuer);
    QCOMPARE(actual.maintenanceCost, expected.maintenanceCost);
    QCOMPARE(actual.observations, expected.observations);
    if (expected.acquisitionType == domain::AcquisitionType::Adquisicion) {
        QCOMPARE(actual.purchasePrice, expected.purchasePrice);
        QCOMPARE(actual.salePrice, expected.salePrice);
        QCOMPARE(actual.paymentType, expected.paymentType);
        QCOMPARE(actual.paymentMethod, expected.paymentMethod);
    } else {
        QCOMPARE(actual.basePrice, expected.basePrice);
        QCOMPARE(actual.commissionRate, expected.commissionRate);
    }
}

} // namespace

class TstVehicleWizardView : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryVehicleRepository m_vehicles;
    fakes::FakeFileStorage m_files;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeContractGenerator m_contracts;
    fakes::InlineTaskRunner m_runner;
    std::unique_ptr<application::VehicleRegistrationService> m_service;
    std::unique_ptr<VehicleWizardView> m_view;
    presentation::VehicleWizardPresenter *m_presenter = nullptr;

    QStackedWidget *stack() const { return m_view->findChild<QStackedWidget *>(); }

    void fillDetails()
    {
        QWidget *page = &m_view->detailsView();
        const QStringList texts{QStringLiteral("model"), QStringLiteral("motorNumber"),
                                QStringLiteral("serialNumber"), QStringLiteral("repuve"),
                                QStringLiteral("plates"), QStringLiteral("platesHolder"),
                                QStringLiteral("counterparty.fullName"),
                                QStringLiteral("counterparty.nationalId"),
                                QStringLiteral("invoiceNumber")};
        for (const QString &field : texts)
            fieldWidget<QLineEdit>(page, field)->setText(QStringLiteral("ABC123"));
        for (const QString &field : {QStringLiteral("vehicleType"), QStringLiteral("subtype"),
                                     QStringLiteral("brand")}) {
            QComboBox *combo = fieldWidget<QComboBox>(page, field);
            QVERIFY2(combo && combo->count() > 0, qPrintable(field));
            combo->setCurrentIndex(0);
        }
        fieldWidget<QDoubleSpinBox>(page, QStringLiteral("purchasePrice"))->setValue(100000);
        fieldWidget<QDoubleSpinBox>(page, QStringLiteral("salePrice"))->setValue(120000);
    }

private slots:
    void init()
    {
        m_vehicles = fakes::InMemoryVehicleRepository();
        m_files = fakes::FakeFileStorage();
        m_service = std::make_unique<application::VehicleRegistrationService>(
            m_vehicles, m_files, m_reference, m_contracts);
        m_view = std::make_unique<VehicleWizardView>();
        m_presenter = new presentation::VehicleWizardPresenter(
            *m_view, m_view->detailsView(), m_view->conditionsView(), m_view->filesView(),
            *m_service, m_runner, false, m_view.get());
        m_view->bind(*m_presenter);
        m_presenter->start();
        m_view->show();
    }

    void cleanup()
    {
        m_view.reset();
        m_service.reset();
    }

    void opensOnDetailsWithCatalogs()
    {
        QCOMPARE(stack()->currentIndex(), 0);
        QVERIFY(buttonWithText(m_view.get(), QStringLiteral("Siguiente")));
        QComboBox *types = fieldWidget<QComboBox>(&m_view->detailsView(), QStringLiteral("vehicleType"));
        QVERIFY(types);
        QCOMPARE(types->count(), 1);
        // Nada en rojo al abrir.
        QLineEdit *vin = fieldWidget<QLineEdit>(&m_view->detailsView(), QStringLiteral("serialNumber"));
        QVERIFY(!vin->property("hasError").toBool());
    }

    void nextOnAnEmptyFormMarksTheFields()
    {
        buttonWithText(m_view.get(), QStringLiteral("Siguiente"))->click();
        QCOMPARE(stack()->currentIndex(), 0);
        QLineEdit *vin = fieldWidget<QLineEdit>(&m_view->detailsView(), QStringLiteral("serialNumber"));
        QVERIFY(vin->property("hasError").toBool());
    }

    // El mensaje de cada campo aparece justo debajo de él, también en la
    // sección de la rama, y el aviso de abajo ya no los repite.
    void errorMessagesAppearUnderTheirFields()
    {
        buttonWithText(m_view.get(), QStringLiteral("Siguiente"))->click();
        settleLayouts();

        QWidget *page = &m_view->detailsView();
        for (const QString &field : {QStringLiteral("serialNumber"), QStringLiteral("purchasePrice")}) {
            QWidget *widget = fieldWidget<QWidget>(page, field);
            auto *message = page->findChild<QLabel *>(widget->objectName() + QStringLiteral("Error"));
            QVERIFY2(message, qPrintable(field));
            QVERIFY2(message->isVisible(), qPrintable(field));
            QVERIFY2(!message->text().isEmpty(), qPrintable(field));

            const QPoint fieldAt = widget->mapTo(page, QPoint(0, 0));
            const QPoint messageAt = message->mapTo(page, QPoint(0, 0));
            QCOMPARE(messageAt.x(), fieldAt.x());
            // Debajo, sin encimarse y sin quedar lejos.
            const int gap = messageAt.y() - (fieldAt.y() + widget->height());
            QVERIFY2(gap >= 0 && gap <= 12, qPrintable(QStringLiteral("%1: %2 px").arg(field).arg(gap)));
        }
        QCOMPARE(m_view->findChild<QLabel *>(QStringLiteral("errorLabel"))->text(),
                 QStringLiteral("Revisa los campos marcados"));
    }

    // Cada paso va en su propia área con scroll: el asistente no pide el alto
    // del paso más grande, así que cabe en una laptop chica.
    void everyStepScrolls()
    {
        const QList<QWidget *> pages{&m_view->detailsView(), &m_view->conditionsView(),
                                     &m_view->filesView()};
        for (QWidget *page : pages) {
            QScrollArea *area = nullptr;
            for (QWidget *widget = page->parentWidget(); widget && !area;
                 widget = widget->parentWidget())
                area = qobject_cast<QScrollArea *>(widget);
            QVERIFY2(area, qPrintable(page->objectName()));
            QCOMPARE(area->widget(), page);
            QVERIFY(area->widgetResizable());
        }
        settleLayouts();
        QVERIFY(stack()->minimumSizeHint().height()
                < m_view->filesView().minimumSizeHint().height());
    }

    // Con errores, el Paso 1 crece un renglón por campo. Va dentro de un área
    // con scroll para que el asistente no pida más alto del que tiene la
    // pantalla: la pila de pasos pide lo mismo con errores que sin ellos.
    void fieldErrorsDoNotGrowTheWizard()
    {
        settleLayouts();
        const int before = stack()->minimumSizeHint().height();
        buttonWithText(m_view.get(), QStringLiteral("Siguiente"))->click();
        settleLayouts();
        QCOMPARE(stack()->minimumSizeHint().height(), before);
    }

    void completeDetailsAdvanceToConditions()
    {
        fillDetails();
        buttonWithText(m_view.get(), QStringLiteral("Siguiente"))->click();
        QCOMPARE(stack()->currentIndex(), 1);
        // El checklist del catálogo falso se armó.
        QVERIFY(!m_view->conditionsView().findChildren<QWidget *>().isEmpty());
    }

    void galleryShowsAcceptedImages()
    {
        const int before = m_view->filesView().findChildren<AspectRatioImageLabel *>().size();
        m_presenter->files().onImagesChosen({QStringLiteral("/fotos/a.png"), QStringLiteral("/fotos/b.png")});
        QCoreApplication::processEvents();
        const int after = m_view->filesView().findChildren<AspectRatioImageLabel *>().size();
        QCOMPARE(after - before, 2);
        QCOMPARE(m_view->filesView().files().images.size(), 2);
        QVERIFY(m_view->filesView().files().images.first().isPrimary);
    }

    void documentsHintShowsTheFormats()
    {
        QLabel *hint = m_view->filesView().findChild<QLabel *>(QStringLiteral("documentFormatsHint"));
        QVERIFY(hint);
        QVERIFY(hint->text().contains(QStringLiteral("PDF")));
    }

    // Cada campo del Paso 1 conserva en "field" la clave con la que el dominio
    // reporta sus errores. Sin ella el campo deja de marcarse en rojo y de
    // recibir el foco cuando falla, sin que nada truene.
    void detailsFieldsKeepTheirKeys()
    {
        QWidget *page = &m_view->detailsView();
        const QStringList keys{
            QStringLiteral("folio"), QStringLiteral("dealDate"), QStringLiteral("vehicleType"),
            QStringLiteral("subtype"), QStringLiteral("brand"), QStringLiteral("model"),
            QStringLiteral("yearModel"), QStringLiteral("color"), QStringLiteral("mileage"),
            QStringLiteral("motorNumber"), QStringLiteral("serialNumber"), QStringLiteral("repuve"),
            QStringLiteral("plates"), QStringLiteral("platesHolder"), QStringLiteral("description"),
            QStringLiteral("acquisitionType"), QStringLiteral("counterparty.fullName"),
            QStringLiteral("counterparty.nationalId"), QStringLiteral("counterparty.streetAddress"),
            QStringLiteral("counterparty.suburb"), QStringLiteral("counterparty.locality"),
            QStringLiteral("counterparty.state"), QStringLiteral("counterparty.postalCode"),
            QStringLiteral("invoiceType"), QStringLiteral("invoiceNumber"),
            QStringLiteral("invoiceIssuer"), QStringLiteral("purchasePrice"),
            QStringLiteral("paymentType"), QStringLiteral("paymentMethod"),
            QStringLiteral("salePrice"), QStringLiteral("basePrice"),
            QStringLiteral("commissionRate"), QStringLiteral("maintenanceCost"),
            QStringLiteral("observations")};
        for (const QString &key : keys)
            QVERIFY2(fieldWidget<QWidget>(page, key), qPrintable(key));
    }

    // Cambiar de rama muestra los campos de una y oculta los de la otra.
    void acquisitionTypeTogglesTheBranchFields()
    {
        QWidget *page = &m_view->detailsView();
        QComboBox *type = fieldWidget<QComboBox>(page, QStringLiteral("acquisitionType"));
        QWidget *purchase = fieldWidget<QWidget>(page, QStringLiteral("purchasePrice"));
        QWidget *base = fieldWidget<QWidget>(page, QStringLiteral("basePrice"));
        QVERIFY(type && type->count() == 2 && purchase && base);
        for (int index : {0, 1, 0}) {
            type->setCurrentIndex(index);
            QVERIFY(purchase->isVisibleTo(page) != base->isVisibleTo(page));
        }
    }

    // La fila del archivo de factura es de la sección de Adquisición: en
    // Consignación no se ve.
    void invoiceFileRowBelongsToAcquisition()
    {
        QWidget *page = &m_view->detailsView();
        QComboBox *type = fieldWidget<QComboBox>(page, QStringLiteral("acquisitionType"));
        auto *upload = page->findChild<QPushButton *>(QStringLiteral("invoiceUploadButton"));
        QVERIFY(type && upload);
        type->setCurrentIndex(0); // Adquisición
        QVERIFY(upload->isVisibleTo(page));
        type->setCurrentIndex(1); // Consignación
        QVERIFY(!upload->isVisibleTo(page));
    }

    // Las secciones de cada rama tienen su propia rejilla, pero sus columnas
    // caen exactamente donde las de la tarjeta.
    void branchSectionsAlignWithTheCard()
    {
        QWidget *page = &m_view->detailsView();
        QComboBox *type = fieldWidget<QComboBox>(page, QStringLiteral("acquisitionType"));
        const auto x = [page](const QString &field) {
            QWidget *widget = fieldWidget<QWidget>(page, field);
            return widget ? widget->mapTo(page, QPoint(0, 0)).x() : -1;
        };

        type->setCurrentIndex(0); // Adquisición
        QCoreApplication::processEvents();
        QCOMPARE(x(QStringLiteral("purchasePrice")), x(QStringLiteral("counterparty.fullName")));
        QCOMPARE(x(QStringLiteral("paymentType")), x(QStringLiteral("counterparty.nationalId")));

        type->setCurrentIndex(1); // Consignación
        QCoreApplication::processEvents();
        QCOMPARE(x(QStringLiteral("basePrice")), x(QStringLiteral("counterparty.fullName")));
        QCOMPARE(x(QStringLiteral("commissionRate")), x(QStringLiteral("counterparty.nationalId")));
    }

    // El orden de tabulación cruza tres formularios: después de "Expidió
    // Factura" vienen los campos de las secciones y luego los comunes del
    // final.
    void tabOrderRunsThroughTheBranchSections()
    {
        QWidget *page = &m_view->detailsView();
        // El siguiente que recibe Tab, sin contar los widgets internos de Qt
        // (el QLineEdit que lleva dentro cada spinbox, por ejemplo).
        const auto nextTabStop = [](QWidget *from) {
            QWidget *next = from->nextInFocusChain();
            while (next != from
                   && (!(next->focusPolicy() & Qt::TabFocus)
                       || next->objectName().startsWith(QStringLiteral("qt_"))))
                next = next->nextInFocusChain();
            return next;
        };
        const auto named = [page](const char *name) {
            return page->findChild<QWidget *>(QString::fromLatin1(name));
        };
        QCOMPARE(nextTabStop(named("invoiceIssuerEdit")), named("cfdiRequestButton"));
        QCOMPARE(nextTabStop(named("salePriceSpin")), named("basePriceSpin"));
        QCOMPARE(nextTabStop(named("commissionRateSpin")), named("maintenanceCostSpin"));
        QCOMPARE(nextTabStop(named("maintenanceCostSpin")), named("observationsEdit"));
    }

    // Los combos del Paso 2 arrancan sin elegir, también los que se llenan
    // desde el dominio después del .ui: así el dominio reporta como faltante
    // lo que nadie eligió.
    void conditionCombosStartUnselected()
    {
        QWidget *page = &m_view->conditionsView();
        for (const QString &field :
             {QStringLiteral("conditions.fuelType"), QStringLiteral("conditions.cylinders"),
              QStringLiteral("conditions.transmission"), QStringLiteral("conditions.interiorMaterial"),
              QStringLiteral("conditions.windowRegulators"),
              QStringLiteral("conditions.airConditioning")}) {
            QComboBox *combo = fieldWidget<QComboBox>(page, field);
            QVERIFY2(combo, qPrintable(field));
            QVERIFY2(combo->count() > 0, qPrintable(field));
            QCOMPARE(combo->currentIndex(), -1);
        }
    }

    // Las tarjetas vienen del .ui con nombre propio (un formulario no admite
    // nombres repetidos) y la vista las renombra a cardPanel, que es lo que
    // busca el QSS. Si alguien quita ese paso, las tarjetas pierden su estilo
    // sin que nada truene.
    // Los campos de texto se detienen en el tope del dominio: un texto más
    // largo lo rechazaría el setter y el usuario no sabría por qué.
    void textFieldsStopAtTheDomainLimits()
    {
        QWidget *page = &m_view->detailsView();
        const struct { const char *field; int limit; } limits[] = {
            {"serialNumber", 50}, {"plates", 20}, {"color", 30},
            {"counterparty.nationalId", 50}, {"counterparty.postalCode", 5},
            {"invoiceIssuer", 150},
        };
        for (const auto &entry : limits) {
            QLineEdit *edit = fieldWidget<QLineEdit>(page, QString::fromLatin1(entry.field));
            QVERIFY2(edit, entry.field);
            QCOMPARE(edit->maxLength(), entry.limit);
        }
    }

    void cardsKeepTheStyleName()
    {
        const auto cards = [](QWidget *page) {
            return page->findChildren<QFrame *>(QStringLiteral("cardPanel"),
                                                Qt::FindDirectChildrenOnly)
                .size();
        };
        QCOMPARE(cards(&m_view->detailsView()), 2);
        QCOMPARE(cards(&m_view->conditionsView()), 2);
        QCOMPARE(cards(&m_view->filesView()), 2);
    }

    // El historial de mantenimientos (US-03.3) va acomodado en el Paso 1,
    // entre las dos tarjetas, pero el alta nunca lo muestra.
    void maintenanceHistoryIsHiddenWhileRegistering()
    {
        QWidget *page = &m_view->detailsView();
        auto *history = page->findChild<MaintenanceHistoryTableWidget *>(
            QStringLiteral("maintenanceHistorySection"));
        QVERIFY(history);
        QCOMPARE(&m_view->detailsView().maintenanceHistory(), history);
        QVERIFY(!history->isVisibleTo(page));

        QLayout *layout = page->layout();
        QCOMPARE(layout->indexOf(history), 1);
        QCOMPARE(layout->itemAt(0)->widget()->objectName(), QStringLiteral("cardPanel"));
        QCOMPARE(layout->itemAt(2)->widget()->objectName(), QStringLiteral("cardPanel"));

        // Las columnas del mockup, en orden.
        auto *table = history->findChild<QTableWidget *>(QStringLiteral("historyTable"));
        QVERIFY(table);
        const QStringList expected{QStringLiteral("Fecha"), QStringLiteral("Tipo de mantenimiento"),
                                   QStringLiteral("Costo"), QStringLiteral("Descripción"),
                                   QStringLiteral("Recibo")};
        QCOMPARE(table->columnCount(), expected.size());
        for (int column = 0; column < expected.size(); ++column)
            QCOMPARE(table->horizontalHeaderItem(column)->text(), expected.at(column));
    }

    void maintenanceHistoryAnnouncesItsButtons()
    {
        MaintenanceHistoryTableWidget &history = m_view->detailsView().maintenanceHistory();
        QSignalSpy added(&history, &MaintenanceHistoryTableWidget::addMaintenanceRequested);
        QSignalSpy printed(&history, &MaintenanceHistoryTableWidget::printReceiptsRequested);
        history.findChild<QPushButton *>(QStringLiteral("addMaintenanceButton"))->click();
        history.findChild<QPushButton *>(QStringLiteral("printReceiptsButton"))->click();
        QCOMPARE(added.size(), 1);
        QCOMPARE(printed.size(), 1);
    }

    // Visible, el historial queda en el Tab entre la descripción y Tipo
    // Operación, igual que en la pantalla.
    void tabOrderRunsThroughTheMaintenanceHistory()
    {
        QWidget *page = &m_view->detailsView();
        m_view->detailsView().setMaintenanceHistoryVisible(true);
        const auto nextTabStop = [](QWidget *from) {
            QWidget *next = from->nextInFocusChain();
            while (next != from
                   && (!(next->focusPolicy() & Qt::TabFocus)
                       || next->objectName().startsWith(QStringLiteral("qt_"))))
                next = next->nextInFocusChain();
            return next;
        };
        const auto named = [page](const char *name) {
            return page->findChild<QWidget *>(QString::fromLatin1(name));
        };
        QCOMPARE(nextTabStop(named("descriptionEdit")), named("addMaintenanceButton"));
        QCOMPARE(nextTabStop(named("addMaintenanceButton")), named("historyTable"));
        QCOMPARE(nextTabStop(named("historyTable")), named("printReceiptsButton"));
        QCOMPARE(nextTabStop(named("printReceiptsButton")), named("acquisitionTypeCombo"));
    }

    // #28: cada paso se pone en solo lectura (el expediente en consulta) y
    // vuelve a ser editable. Los valores no cambian; los botones de captura
    // se ocultan y "Ver" se queda.
    void stepsTurnReadOnlyAndBack()
    {
        QWidget *details = &m_view->detailsView();
        QWidget *conditions = &m_view->conditionsView();
        QWidget *files = &m_view->filesView();
        auto *vin = fieldWidget<QLineEdit>(details, QStringLiteral("serialNumber"));
        vin->setText(QStringLiteral("VIN123"));
        auto *brand = fieldWidget<QComboBox>(details, QStringLiteral("brand"));
        auto *fuel = fieldWidget<QComboBox>(conditions, QStringLiteral("conditions.fuelType"));
        auto *upload = details->findChild<QPushButton *>(QStringLiteral("invoiceUploadButton"));
        auto *addPhoto = files->findChild<QPushButton *>(QStringLiteral("addPhotoButton"));
        const auto visibleButtons = [](QWidget *root, const QString &text) {
            int count = 0;
            for (QPushButton *button : root->findChildren<QPushButton *>()) {
                if (button->text() == text && button->isVisibleTo(root))
                    ++count;
            }
            return count;
        };
        QVERIFY(visibleButtons(conditions, QStringLiteral("Marcar todo")) > 0);
        QVERIFY(visibleButtons(files, QStringLiteral("Subir documento")) > 0);

        m_view->detailsView().setReadOnly(true, {});
        m_view->conditionsView().setReadOnly(true, {});
        m_view->filesView().setReadOnly(true, {});
        QVERIFY(vin->isReadOnly());
        QCOMPARE(vin->text(), QStringLiteral("VIN123"));
        QVERIFY(brand->property("readOnly").toBool());
        QVERIFY(brand->lineEdit()->isReadOnly());
        QVERIFY(!upload->isVisibleTo(details));
        QVERIFY(fuel->property("readOnly").toBool());
        QCOMPARE(visibleButtons(conditions, QStringLiteral("Marcar todo")), 0);
        for (QCheckBox *check : conditions->findChildren<QCheckBox *>())
            QVERIFY(check->property("readOnly").toBool());
        QVERIFY(!addPhoto->isVisibleTo(files));
        QVERIFY(!files->acceptDrops());
        QCOMPARE(visibleButtons(files, QStringLiteral("Subir documento")), 0);
        // Las filas viejas se retiran con deleteLater(): se borran antes de revisar.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (QCheckBox *check : files->findChildren<QCheckBox *>())
            QVERIFY(check->property("readOnly").toBool());
        // El historial de mantenimientos no es parte de la captura: un editor
        // suyo (como los de su alta en línea) sigue editable.
        auto *history = m_view->detailsView().maintenanceHistory().findChild<QTableWidget *>();
        history->setRowCount(1);
        auto *historyCell = new QLineEdit;
        history->setCellWidget(0, 0, historyCell);
        m_view->detailsView().setReadOnly(true, {});
        QVERIFY(!historyCell->isReadOnly());

        m_view->detailsView().setReadOnly(false, {});
        m_view->conditionsView().setReadOnly(false, {});
        m_view->filesView().setReadOnly(false, {});
        QVERIFY(!vin->isReadOnly());
        QVERIFY(!brand->property("readOnly").toBool());
        QVERIFY(upload->isVisibleTo(details));
        QVERIFY(visibleButtons(conditions, QStringLiteral("Marcar todo")) > 0);
        QVERIFY(addPhoto->isVisibleTo(files));
        QVERIFY(files->acceptDrops());
        QVERIFY(visibleButtons(files, QStringLiteral("Subir documento")) > 0);
    }

    // Lo que un paso arma en código después de ponerse en solo lectura nace
    // igual: el checklist que llega con los catálogos y las filas de
    // documentos que se rehacen al adjuntar uno. Volver a armar el checklist
    // tampoco duplica filas.
    void rebuiltRowsStayReadOnly()
    {
        QWidget *conditions = &m_view->conditionsView();
        m_view->conditionsView().setReadOnly(true, {});
        m_view->conditionsView().setChecklist(
            {{7, QStringLiteral("Exterior"), QStringLiteral("Llantas")},
             {8, QStringLiteral("Exterior"), QStringLiteral("Faros")}});
        const QList<QCheckBox *> checks = conditions->findChildren<QCheckBox *>();
        QCOMPARE(checks.size(), 2);
        for (QCheckBox *check : checks)
            QVERIFY(check->property("readOnly").toBool());
        for (QPushButton *button : conditions->findChildren<QPushButton *>())
            QVERIFY(!button->isVisibleTo(conditions));

        QWidget *files = &m_view->filesView();
        m_view->filesView().setReadOnly(true, {});
        m_view->filesView().attachDocument(QStringLiteral("Placas"), QStringLiteral("C:/x/placas.pdf"));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        auto *view = files->findChild<QPushButton *>(QStringLiteral("documentViewButton"));
        QVERIFY(view);
        QVERIFY(!buttonWithText(files, QStringLiteral("Reemplazar")));
        for (QCheckBox *check : files->findChildren<QCheckBox *>())
            QVERIFY(check->property("readOnly").toBool());
    }

    // El presenter de la factura puede repintar su fila en cualquier momento;
    // en solo lectura sus botones siguen ocultos, y al volver a editar se ven
    // según su último estado.
    void invoiceButtonsStayHiddenWhileReadOnly()
    {
        QWidget *details = &m_view->detailsView();
        auto *cfdi = details->findChild<QPushButton *>(QStringLiteral("cfdiRequestButton"));
        auto *upload = details->findChild<QPushButton *>(QStringLiteral("invoiceUploadButton"));
        m_view->detailsView().setReadOnly(true, {});

        presentation::InvoiceAttachmentState state;
        state.cfdiButtonVisible = true;
        state.uploadEnabled = false;
        m_view->detailsView().acquisitionTerms().showInvoiceAttachment(state);
        QVERIFY(!cfdi->isVisibleTo(details));
        QVERIFY(!upload->isVisibleTo(details));

        m_view->detailsView().setReadOnly(false, {});
        QVERIFY(cfdi->isVisibleTo(details));
        QVERIFY(upload->isVisibleTo(details));
        QVERIFY(!upload->isEnabled());
    }

    // "Ver" aparece solo con una factura adjunta, también en consulta, y el
    // Tab pasa por él después de "Subir documento".
    void invoiceViewButtonFollowsTheAttachment()
    {
        QWidget *details = &m_view->detailsView();
        auto *view = details->findChild<QPushButton *>(QStringLiteral("invoiceViewButton"));
        QVERIFY(view);
        QVERIFY(!view->isVisibleTo(details));

        presentation::InvoiceAttachmentState state;
        state.fileLabel = QStringLiteral("factura.pdf");
        state.viewVisible = true;
        m_view->detailsView().acquisitionTerms().showInvoiceAttachment(state);
        QVERIFY(view->isVisibleTo(details));
        m_view->detailsView().setReadOnly(true, {});
        QVERIFY(view->isVisibleTo(details));

        QSignalSpy clicked(&m_view->detailsView(), &VehicleDetailsView::viewInvoiceRequested);
        view->click();
        QCOMPARE(clicked.size(), 1);

        // El siguiente que recibe Tab, sin contar los widgets internos de Qt.
        const auto nextTabStop = [](QWidget *from) {
            QWidget *next = from->nextInFocusChain();
            while (next != from
                   && (!(next->focusPolicy() & Qt::TabFocus)
                       || next->objectName().startsWith(QStringLiteral("qt_"))))
                next = next->nextInFocusChain();
            return next;
        };
        auto *upload = details->findChild<QWidget *>(QStringLiteral("invoiceUploadButton"));
        QWidget *purchase = fieldWidget<QDoubleSpinBox>(details, QStringLiteral("purchasePrice"));
        QCOMPARE(nextTabStop(upload), static_cast<QWidget *>(view));
        QCOMPARE(nextTabStop(view), purchase);
    }

    // Lo guardado se pinta tal cual y details() lo devuelve igual, en las dos
    // ramas: así lee el expediente lo que el usuario corrige.
    void storedDetailsComeBackUnchanged_data()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("compra") << static_cast<int>(domain::AcquisitionType::Adquisicion);
        QTest::newRow("consignación") << static_cast<int>(domain::AcquisitionType::Consignacion);
    }

    void storedDetailsComeBackUnchanged()
    {
        QFETCH(int, type);
        const application::VehicleDetailsDto stored =
            completeDetails(static_cast<domain::AcquisitionType>(type));

        m_view->detailsView().showDetails(12, stored);
        compareDetails(m_view->detailsView().details(), stored);
        if (QTest::currentTestFailed())
            return;
        auto *folio = fieldWidget<QLineEdit>(&m_view->detailsView(), QStringLiteral("folio"));
        QCOMPARE(folio->text(), QStringLiteral("12"));
        QVERIFY(!folio->isEnabled());
    }

    // Un dato guardado que el formulario no ofrecería al capturar se pinta
    // igual: el rango se amplía y la opción que falta se agrega, en vez de
    // cambiar el dato.
    void storedValuesOutsideTheFormStayAsTheyAre()
    {
        application::VehicleDetailsDto stored = completeDetails(domain::AcquisitionType::Adquisicion);
        stored.dealDate = QDate(1999, 5, 1);
        stored.yearModel = 1975;
        stored.mileage = 2500000;
        stored.maintenanceCost = 12000000.5;
        stored.purchasePrice = 150000000.0;
        stored.salePrice = 160000000.0;
        stored.brand = {99, QStringLiteral("Datsun")};

        m_view->detailsView().showDetails(3, stored);
        compareDetails(m_view->detailsView().details(), stored);
        if (QTest::currentTestFailed())
            return;
        // El total de mantenimientos se puede repintar solo.
        m_view->detailsView().showMaintenanceCost(12000100.0);
        QCOMPARE(m_view->detailsView().details().maintenanceCost, 12000100.0);
    }

    // Lo guardado del Paso 2 vuelve igual. Si llega antes que el checklist,
    // se aplica al armarlo: la fila que no viene queda desmarcada.
    void storedConditionsComeBackUnchanged()
    {
        application::VehicleConditionsDto stored =
            fixtures::validRegistration(domain::AcquisitionType::Adquisicion).conditions;
        stored.cylinders = 12; // no está entre las opciones del .ui
        stored.interiorMaterial = QStringLiteral("Piel");
        stored.windowRegulators = domain::WindowRegulators::ElectricosInteligentes;
        stored.inspection = {{8, false, QStringLiteral("Uno estrellado")}};

        VehicleConditionsView view;
        view.setFuelTypes({{1, QStringLiteral("Gasolina"), -1}});
        view.showConditions(stored);
        view.setChecklist({{7, QStringLiteral("Exterior"), QStringLiteral("Llantas")},
                           {8, QStringLiteral("Exterior"), QStringLiteral("Faros")}});

        const application::VehicleConditionsDto shown = view.conditions();
        QCOMPARE(shown.fuelType.id, stored.fuelType.id);
        QCOMPARE(shown.cylinders, stored.cylinders);
        QCOMPARE(shown.transmission, stored.transmission);
        QCOMPARE(shown.interiorMaterial, stored.interiorMaterial);
        QCOMPARE(shown.windowRegulators, stored.windowRegulators);
        QCOMPARE(shown.airConditioning, stored.airConditioning);
        QCOMPARE(shown.inspection.size(), 1);
        QCOMPARE(shown.inspection.first().elementId, 8);
        QVERIFY(!shown.inspection.first().isOptimal);
        QCOMPARE(shown.inspection.first().observations, QStringLiteral("Uno estrellado"));

        // Con las filas ya armadas se aplica en el acto.
        stored.inspection = {{7, true, QString()}};
        view.showConditions(stored);
        QCOMPARE(view.conditions().inspection.size(), 1);
        QCOMPARE(view.conditions().inspection.first().elementId, 7);
    }

    // Un texto guardado más largo que su campo se ve desde el principio, no
    // cortado por el inicio.
    void longStoredTextShowsItsStart()
    {
        application::VehicleDetailsDto stored = completeDetails(domain::AcquisitionType::Adquisicion);
        stored.counterparty.streetAddress =
            QStringLiteral("Avenida Francisco I. Madero Poniente 1234, interior 5, edificio B");
        m_view->detailsView().showDetails(3, stored);
        auto *street = fieldWidget<QLineEdit>(&m_view->detailsView(),
                                              QStringLiteral("counterparty.streetAddress"));
        QCOMPARE(street->cursorPosition(), 0);
        QCOMPARE(street->text(), stored.counterparty.streetAddress);
    }

    // También el número de póliza del Seguro, que se arma en código.
    void longPolicyNumberShowsItsStart()
    {
        const QString policy = QStringLiteral("POLIZA-0123456789-ABCDEFGHIJ");
        m_view->filesView().showFiles({}, {{QStringLiteral("Seguro"),
                                            QStringLiteral("C:/store/seguro.pdf"), policy}});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        auto *edit = m_view->filesView().findChild<QLineEdit *>(QStringLiteral("policyNumberEdit"));
        QCOMPARE(edit->text(), policy);
        QCOMPARE(edit->cursorPosition(), 0);
    }

    // Si el catálogo de condiciones llega en una lectura posterior, el aviso
    // de que no estaba disponible desaparece.
    void checklistNoticeGoesAwayWhenTheCatalogArrives()
    {
        VehicleConditionsView view;
        view.showChecklistMessage(QStringLiteral("El catálogo no está disponible."));
        auto *notice = view.findChild<QLabel *>(QStringLiteral("checklistMessageLabel"));
        QVERIFY(notice->isVisibleTo(&view));
        view.setChecklist({{7, QStringLiteral("Exterior"), QStringLiteral("Llantas")}});
        QVERIFY(!notice->isVisibleTo(&view));
    }

    // Lo guardado del Paso 3 reemplaza lo que hubiera, con su portada, y no
    // arrastra casillas ni avisos de antes.
    void storedFilesReplaceWhatWasThere()
    {
        auto &view = m_view->filesView();
        QWidget *files = &view;
        view.addImages({{QStringLiteral("C:/nuevas/a.jpg"), QByteArray()}});
        view.showGalleryMessage(QStringLiteral("No se agregó este archivo"));
        view.showDocumentsMessage(QStringLiteral("Formato no permitido"));
        documentCheck(files, QStringLiteral("Placas"))->setChecked(true);

        const QString card = QStringLiteral("Tarjeta de circulación");
        const QString insurance = QStringLiteral("Seguro");
        view.showFiles({{QStringLiteral("C:/store/1.jpg"), QByteArray(), false},
                        {QStringLiteral("C:/store/2.jpg"), QByteArray(), true}},
                       {{card, QStringLiteral("C:/store/tarjeta.pdf"), QString()},
                        {insurance, QStringLiteral("C:/store/seguro.pdf"), QStringLiteral("POL-9")}});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        const application::VehicleFilesDto shown = view.files();
        QCOMPARE(shown.images.size(), 2);
        QCOMPARE(shown.images.at(0).path, QStringLiteral("C:/store/1.jpg"));
        QVERIFY(!shown.images.at(0).isPrimary);
        QVERIFY(shown.images.at(1).isPrimary);
        QCOMPARE(shown.documents.size(), 2);
        for (const domain::VehicleDocument &document : shown.documents) {
            if (document.documentType == insurance)
                QCOMPARE(document.documentNumber, QStringLiteral("POL-9"));
            else
                QCOMPARE(document.path, QStringLiteral("C:/store/tarjeta.pdf"));
        }

        QVERIFY(!documentCheck(files, QStringLiteral("Placas"))->isChecked());
        QVERIFY(documentCheck(files, card)->isChecked());
        QVERIFY(!files->findChild<QLabel *>(QStringLiteral("galleryErrorLabel"))->isVisibleTo(files));
        QVERIFY(!files->findChild<QLabel *>(QStringLiteral("documentsErrorLabel"))->isVisibleTo(files));
        QCOMPARE(files->findChild<QLineEdit *>(QStringLiteral("policyNumberEdit"))->text(),
                 QStringLiteral("POL-9"));
    }
};

QTEST_MAIN(TstVehicleWizardView)
#include "tst_vehiclewizardview.moc"
