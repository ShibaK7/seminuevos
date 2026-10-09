#ifndef TESTS_SUPPORT_WIZARDFAKES_H
#define TESTS_SUPPORT_WIZARDFAKES_H

// Vistas falsas del asistente de registro. Implementan las mismas interfaces
// que los widgets reales y anotan lo que el presenter les pidió, así que la
// navegación, la validación y el registro se prueban sin ventanas.

#include "presentation/inventory/registration/steps/conditions/ivehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/ivehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/ivehiclefilesview.h"
#include "presentation/inventory/registration/ivehiclewizardview.h"

#include <QStringList>

namespace fakes {

// Lo común de las tres páginas: qué errores se marcaron y qué campo pidió el
// foco.
template <class Interface>
class FakeStepView : public Interface
{
public:
    QList<domain::ValidationError> markedErrors;
    int showErrorsCalls = 0;
    QStringList focusRequests;
    // Campos que la vista "no tiene" en pantalla: focusField() responde false.
    QStringList hiddenFields;

    void showFieldErrors(const QList<domain::ValidationError> &errors) override
    {
        ++showErrorsCalls;
        markedErrors = errors;
    }
    bool focusField(const QString &field) override
    {
        if (hiddenFields.contains(field))
            return false;
        focusRequests << field;
        return true;
    }

    QStringList markedFields() const
    {
        QStringList fields;
        for (const domain::ValidationError &error : markedErrors)
            fields << error.field;
        return fields;
    }
};

class FakeDetailsView final : public FakeStepView<presentation::IVehicleDetailsView>
{
public:
    application::VehicleDetailsDto captured;
    application::RegistrationLookupsDto lookups;
    presentation::InvoiceAttachmentState attachment;
    QString nextInvoiceFile;
    bool openSucceeds = true;
    QStringList opened;
    QStringList warnings;

    application::VehicleDetailsDto details() const override { return captured; }
    void setLookups(const application::RegistrationLookupsDto &value) override { lookups = value; }
    void showInvoiceAttachment(const presentation::InvoiceAttachmentState &state) override
    {
        attachment = state;
    }
    QString askInvoiceFile() override { return nextInvoiceFile; }
    bool openDocument(const QString &path) override
    {
        opened << path;
        return openSucceeds;
    }
    void showWarning(const QString &, const QString &message) override { warnings << message; }
};

class FakeConditionsView final : public FakeStepView<presentation::IVehicleConditionsView>
{
public:
    application::VehicleConditionsDto captured;
    QList<application::CatalogOptionDto> fuelTypes;
    QList<application::ChecklistItemDto> checklist;
    QString checklistMessage;

    application::VehicleConditionsDto conditions() const override { return captured; }
    void setFuelTypes(const QList<application::CatalogOptionDto> &value) override { fuelTypes = value; }
    void setChecklist(const QList<application::ChecklistItemDto> &items) override { checklist = items; }
    void showChecklistMessage(const QString &message) override { checklistMessage = message; }
};

class FakeFilesView final : public FakeStepView<presentation::IVehicleFilesView>
{
public:
    application::VehicleFilesDto captured;
    application::UploadFormatsDto imageFormats;
    application::UploadFormatsDto documentFormats;
    QList<presentation::ImagePreview> addedImages;
    QString galleryMessage;
    QString documentsMessage;
    QMap<QString, QString> attachedDocuments;

    application::VehicleFilesDto files() const override { return captured; }
    void setUploadFormats(const application::UploadFormatsDto &images,
                          const application::UploadFormatsDto &documents) override
    {
        imageFormats = images;
        documentFormats = documents;
    }
    void addImages(const QList<presentation::ImagePreview> &images) override { addedImages << images; }
    void showGalleryMessage(const QString &message) override { galleryMessage = message; }
    void attachDocument(const QString &documentType, const QString &path) override
    {
        attachedDocuments.insert(documentType, path);
    }
    void showDocumentsMessage(const QString &message) override { documentsMessage = message; }
};

class FakeWizardView final : public presentation::IVehicleWizardView
{
public:
    int shownStep = -1;
    QList<presentation::StepIndicator> indicators;
    QString message;
    presentation::PrimaryAction primaryAction = presentation::PrimaryAction::Loading;
    bool busy = false;
    bool registered = false;
    bool canPrint = false;
    presentation::AfterRegistration afterRegistration = presentation::AfterRegistration::BackToInventory;
    QList<int> announcedFolios;
    QString contractPath;
    QStringList opened;
    QStringList contractErrors;
    bool discard = true;
    int discardQuestions = 0;

    void showStep(int step) override { shownStep = step; }
    void showStepIndicators(const QList<presentation::StepIndicator> &value) override
    {
        indicators = value;
    }
    void showMessage(const QString &value) override { message = value; }
    void setPrimaryAction(presentation::PrimaryAction action) override { primaryAction = action; }
    void setBusy(bool value) override { busy = value; }
    void showRegistered(bool canPrintContract) override
    {
        registered = true;
        canPrint = canPrintContract;
    }
    presentation::AfterRegistration askAfterRegistration(int folio, bool) override
    {
        announcedFolios << folio;
        return afterRegistration;
    }
    QString askContractPath(const QString &) override { return contractPath; }
    void openDocument(const QString &path) override { opened << path; }
    void showContractError(const QString &value) override { contractErrors << value; }
    bool confirmDiscard() override
    {
        ++discardQuestions;
        return discard;
    }
};

} // namespace fakes

#endif // TESTS_SUPPORT_WIZARDFAKES_H
