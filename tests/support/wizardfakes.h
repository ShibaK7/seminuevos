#ifndef TESTS_SUPPORT_WIZARDFAKES_H
#define TESTS_SUPPORT_WIZARDFAKES_H

// Vistas falsas del asistente de registro. Implementan las mismas interfaces
// que los widgets reales y anotan lo que el presenter les pidió, así que la
// navegación, la validación y el registro se prueban sin ventanas.

#include "presentation/inventory/record/ivehiclerecordview.h"
#include "presentation/inventory/registration/steps/conditions/ivehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/ivehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/ivehiclefilesview.h"
#include "presentation/inventory/registration/ivehiclewizardview.h"

#include <QStringList>

#include <optional>

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
    // Campos que la vista "no tiene" en pantalla: focusField() responde false
    // y showFieldErrors() devuelve sus errores como no mostrados.
    QStringList hiddenFields;
    // El último modo que pidió el presenter (nullopt = nunca lo pidió).
    std::optional<bool> readOnly;
    QStringList lockedFields;

    void setReadOnly(bool value, const QStringList &locked) override
    {
        readOnly = value;
        lockedFields = locked;
    }

    QList<domain::ValidationError>
    showFieldErrors(const QList<domain::ValidationError> &errors) override
    {
        ++showErrorsCalls;
        markedErrors = errors;
        QList<domain::ValidationError> unshown;
        for (const domain::ValidationError &error : errors) {
            if (hiddenFields.contains(error.field))
                unshown << error;
        }
        return unshown;
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

// La sección de Adquisición del Paso 1: la fila de la factura.
class FakeAcquisitionTermsView final : public presentation::IAcquisitionTermsView
{
public:
    presentation::InvoiceAttachmentState attachment;
    QString nextInvoiceFile;
    bool openSucceeds = true;
    QStringList opened;
    QStringList warnings;

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

class FakeDetailsView final : public FakeStepView<presentation::IVehicleDetailsView>
{
public:
    application::VehicleDetailsDto captured;
    application::RegistrationLookupsDto lookups;
    FakeAcquisitionTermsView acquisition;
    // Lo último que se pintó de una unidad guardada.
    std::optional<int> shownFolio;
    std::optional<application::VehicleDetailsDto> shown;
    std::optional<double> maintenanceCost;
    int showDetailsCalls = 0;
    int setLookupsCalls = 0;

    application::VehicleDetailsDto details() const override { return captured; }
    void setLookups(const application::RegistrationLookupsDto &value) override
    {
        ++setLookupsCalls;
        lookups = value;
    }
    // Como la vista real: lo pintado es lo que details() devuelve después.
    void showDetails(int folio, const application::VehicleDetailsDto &value) override
    {
        ++showDetailsCalls;
        shownFolio = folio;
        shown = value;
        captured = value;
        captured.invoiceFilePath.clear();
        maintenanceCost = value.maintenanceCost;
    }
    void showMaintenanceCost(double cost) override
    {
        maintenanceCost = cost;
        captured.maintenanceCost = cost;
    }
    presentation::IAcquisitionTermsView &acquisitionTerms() override { return acquisition; }
};

class FakeConditionsView final : public FakeStepView<presentation::IVehicleConditionsView>
{
public:
    application::VehicleConditionsDto captured;
    QList<application::CatalogOptionDto> fuelTypes;
    QList<application::ChecklistItemDto> checklist;
    QString checklistMessage;

    std::optional<application::VehicleConditionsDto> shown;
    int setChecklistCalls = 0;

    application::VehicleConditionsDto conditions() const override { return captured; }
    void setFuelTypes(const QList<application::CatalogOptionDto> &value) override { fuelTypes = value; }
    void setChecklist(const QList<application::ChecklistItemDto> &items) override
    {
        ++setChecklistCalls;
        checklist = items;
    }
    void showChecklistMessage(const QString &message) override { checklistMessage = message; }
    void showConditions(const application::VehicleConditionsDto &value) override
    {
        shown = value;
        captured = value;
    }
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
    // Lo último que se pintó de una unidad guardada.
    QList<presentation::ImagePreview> shownImages;
    QList<domain::VehicleDocument> shownDocuments;
    int showFilesCalls = 0;
    void showFiles(const QList<presentation::ImagePreview> &images,
                   const QList<domain::VehicleDocument> &documents) override
    {
        ++showFilesCalls;
        shownImages = images;
        shownDocuments = documents;
        captured = application::VehicleFilesDto();
        for (const presentation::ImagePreview &image : images)
            captured.images << domain::VehicleImage{image.path, image.isPrimary};
        captured.documents = documents;
        galleryMessage.clear();
        documentsMessage.clear();
    }
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

// El marco del expediente: anota título, pestaña, estados, avisos y lo que el
// rol deja ver.
class FakeRecordView final : public presentation::IVehicleRecordView
{
public:
    QString title;
    int shownStep = -1;
    QList<presentation::StepVisual> states;
    QString message;
    QString notice;
    bool busy = false;
    std::optional<bool> dealTermsVisible;
    std::optional<bool> maintenanceHistoryVisible;

    void showTitle(const QString &value) override { title = value; }
    void showStep(int step) override { shownStep = step; }
    void showStepStates(const QList<presentation::StepVisual> &value) override { states = value; }
    void showMessage(const QString &value) override { message = value; }
    void showNotice(const QString &value) override { notice = value; }
    void setBusy(bool value) override { busy = value; }
    void setDealTermsVisible(bool visible) override { dealTermsVisible = visible; }
    void setMaintenanceHistoryVisible(bool visible) override { maintenanceHistoryVisible = visible; }
};

} // namespace fakes

#endif // TESTS_SUPPORT_WIZARDFAKES_H
