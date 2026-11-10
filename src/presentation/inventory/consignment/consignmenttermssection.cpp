#include "presentation/inventory/consignment/consignmenttermssection.h"
#include "ui_consignmenttermssection.h"

#include "presentation/common/forms/formsupport.h"

#include <QDoubleSpinBox>

ConsignmentTermsSection::ConsignmentTermsSection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConsignmentTermsSection)
{
    ui->setupUi(this);
}

ConsignmentTermsSection::~ConsignmentTermsSection()
{
    delete ui;
}

void ConsignmentTermsSection::fill(application::VehicleDetailsDto &dto) const
{
    dto.basePrice = ui->basePriceSpin->value();
    dto.commissionRate = ui->commissionRateSpin->value();
}

void ConsignmentTermsSection::showTerms(const application::VehicleDetailsDto &dto)
{
    formsupport::showValue(ui->basePriceSpin, dto.basePrice);
    formsupport::showValue(ui->commissionRateSpin, dto.commissionRate);
}

QList<QWidget *> ConsignmentTermsSection::focusOrder() const
{
    return {ui->basePriceSpin, ui->commissionRateSpin};
}

QGridLayout *ConsignmentTermsSection::grid() const
{
    return ui->consignmentTermsGrid;
}
