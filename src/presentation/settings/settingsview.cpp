#include "presentation/settings/settingsview.h"
#include "ui_settingsview.h"

#include "presentation/settings/cashbalancespresenter.h"
#include "presentation/settings/cashbalanceswidget.h"
#include "presentation/settings/systemsettingspresenter.h"
#include "presentation/settings/systemsettingswidget.h"
#include "presentation/settings/usermanagementpresenter.h"
#include "presentation/settings/usermanagementwidget.h"

SettingsView::SettingsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsView)
{
    ui->setupUi(this);
}

SettingsView::~SettingsView()
{
    // Los presenters son hijos de esta página y viven más que las vistas de
    // ui: se borran antes, para que nada les hable a widgets ya destruidos.
    delete m_parameters.data();
    delete m_balances.data();
    delete m_users.data();
    delete ui;
}

SystemSettingsWidget &SettingsView::systemSettings()
{
    return *ui->systemSettings;
}

CashBalancesWidget &SettingsView::cashBalances()
{
    return *ui->cashBalances;
}

UserManagementWidget &SettingsView::userManagement()
{
    return *ui->userManagement;
}

void SettingsView::bind(presentation::SystemSettingsPresenter &parameters,
                        presentation::CashBalancesPresenter &balances,
                        presentation::UserManagementPresenter &users)
{
    m_parameters = &parameters;
    m_balances = &balances;
    m_users = &users;
    ui->systemSettings->bind(parameters);
    ui->cashBalances->bind(balances);
    ui->userManagement->bind(users);
}

void SettingsView::refresh()
{
    if (m_parameters)
        m_parameters->refresh();
    if (m_balances)
        m_balances->refresh();
    if (m_users)
        m_users->refresh();
}

bool SettingsView::confirmLeave()
{
    if (m_balances && !m_balances->confirmLeave())
        return false;
    return !m_parameters || m_parameters->confirmLeave();
}
