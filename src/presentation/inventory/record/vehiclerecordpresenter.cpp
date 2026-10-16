#include "presentation/inventory/record/vehiclerecordpresenter.h"

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/inventory/record/ivehiclerecordview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionspresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"

namespace presentation {

namespace {

constexpr int kStepCount = 3;

} // namespace

VehicleRecordPresenter::VehicleRecordPresenter(IVehicleRecordView &record,
                                               IVehicleDetailsView &details,
                                               IVehicleConditionsView &conditions,
                                               IVehicleFilesView &files,
                                               const application::VehicleRegistrationService &forms,
                                               const application::VehicleRecordService &records,
                                               TaskRunner &runner, int folio, domain::UserRole role,
                                               QObject *parent)
    : QObject(parent)
    , m_record(record)
    , m_forms(forms)
    , m_records(records)
    , m_runner(runner)
    , m_folio(folio)
    , m_role(role)
    , m_details(std::make_unique<VehicleDetailsPresenter>(details, forms))
    , m_conditions(std::make_unique<VehicleConditionsPresenter>(conditions, forms))
    , m_files(std::make_unique<VehicleFilesPresenter>(files, forms))
    , m_nav(kStepCount, true)
{
    m_steps = {m_details.get(), m_conditions.get(), m_files.get()};
}

VehicleRecordPresenter::~VehicleRecordPresenter() = default;

VehicleDetailsPresenter &VehicleRecordPresenter::details()
{
    return *m_details;
}

VehicleFilesPresenter &VehicleRecordPresenter::files()
{
    return *m_files;
}

int VehicleRecordPresenter::folio() const
{
    return m_folio;
}

void VehicleRecordPresenter::start()
{
    // Lo que el rol no deja ver se decide antes de pintar nada, para que no
    // se asome ni un instante.
    m_access = m_records.accessFor(m_role);
    m_record.setDealTermsVisible(m_access.showsDealTerms);
    m_record.setMaintenanceHistoryVisible(m_access.showsMaintenanceHistory);
    m_record.showTitle(QStringLiteral("Folio %1").arg(m_folio));
    showCurrentStep();
}

void VehicleRecordPresenter::refresh()
{
    // Todavía no hay nada que releer: el expediente aún no lee la base.
}

void VehicleRecordPresenter::onStepClicked(int step)
{
    if (step < 0 || step >= kStepCount || step == m_nav.current())
        return;
    m_nav.moveTo(step);
    showCurrentStep();
}

void VehicleRecordPresenter::onBack()
{
    if (confirmLeave())
        emit closeRequested();
}

bool VehicleRecordPresenter::confirmLeave()
{
    // En consulta no hay nada que se pueda perder.
    return true;
}

void VehicleRecordPresenter::showCurrentStep()
{
    m_record.showStep(m_nav.current());
    refreshStepper();
}

void VehicleRecordPresenter::refreshStepper()
{
    // Pestañas, no pasos: la actual y las demás, sin candados ni palomitas.
    QList<StepVisual> states;
    for (int step = 0; step < kStepCount; ++step)
        states << (step == m_nav.current() ? StepVisual::Current : StepVisual::Pending);
    m_record.showStepStates(states);
}

} // namespace presentation
