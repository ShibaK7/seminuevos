#include "presentation/inventory/consignment/consignmenttermssection.h"
#include "ui_consignmenttermssection.h"

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

QList<QWidget *> ConsignmentTermsSection::focusOrder() const
{
    return {ui->basePriceSpin, ui->commissionRateSpin};
}

QGridLayout *ConsignmentTermsSection::grid() const
{
    return ui->consignmentTermsGrid;
}
