// Prueba de humo del asistente con sus widgets reales: la vista, sus tres
// páginas y el presenter, sobre puertos falsos. Cubre lo que las vistas falsas
// no ven: que bind() conecte de verdad, que las páginas pinten lo que el
// presenter manda y que nada truene al recorrer los pasos.

#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"
#include "presentation/inventory/registration/vehiclewizardpresenter.h"
#include "presentation/common/components/aspectratioimagelabel.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"
#include "presentation/inventory/registration/vehiclewizardview.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
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

QPushButton *buttonWithText(QWidget *root, const QString &text)
{
    for (QPushButton *button : root->findChildren<QPushButton *>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
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
};

QTEST_MAIN(TstVehicleWizardView)
#include "tst_vehiclewizardview.moc"
