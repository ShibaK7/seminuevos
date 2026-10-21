//
// Created by Alucard on 14/10/2026.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CommercialMainView.h" resolved

#include "commercialmainview.h"
#include "ui_CommercialMainView.h"


CommercialMainView::CommercialMainView(QWidget* parent) :
    QWidget(parent), ui(new Ui::CommercialMainView)
{
    ui->setupUi(this);
    ui->salesTab->click();
    ui->newSaleButton->setIcon(QIcon(":/icons/plus.png"));
    ui->newSaleButton->setIconSize(QSize(20,20));
    ui->newSaleButton->hide();
}

CommercialMainView::~CommercialMainView()
{
    delete ui;
}
