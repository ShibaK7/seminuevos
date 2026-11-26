#include "presentation/settings/usermanagementwidget.h"
#include "ui_usermanagementwidget.h"

#include "presentation/common/forms/formsupport.h"
#include "presentation/settings/useraccountdialog.h"
#include "presentation/settings/usermanagementpresenter.h"

#include <QHeaderView>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>

namespace {

// El id de cada cuenta va en la celda del usuario.
constexpr int kIdRole = Qt::UserRole;

QTableWidgetItem *cell(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

} // namespace

UserManagementWidget::UserManagementWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UserManagementWidget)
{
    ui->setupUi(this);
    // La sombra no cabe en el .ui: Designer no la declara.
    formsupport::applyFloatingShadow(ui->usersCard);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);

    connect(ui->addUserButton, &QPushButton::clicked, this, &UserManagementWidget::addUserRequested);
    connect(ui->editUserButton, &QPushButton::clicked, this, &UserManagementWidget::editUserRequested);
    connect(ui->resetPasswordButton, &QPushButton::clicked, this,
            &UserManagementWidget::resetPasswordRequested);
    connect(ui->toggleActiveButton, &QPushButton::clicked, this,
            &UserManagementWidget::toggleActiveRequested);
    connect(ui->usersTable, &QTableWidget::itemDoubleClicked, this,
            &UserManagementWidget::editUserRequested);
    connect(ui->usersTable, &QTableWidget::itemSelectionChanged, this,
            &UserManagementWidget::updateSelectionButtons);

    showMessage(QString());
    showNotice(QString());
    updateSelectionButtons();
}

UserManagementWidget::~UserManagementWidget()
{
    delete ui;
}

void UserManagementWidget::bind(presentation::UserManagementPresenter &presenter)
{
    using presentation::UserManagementPresenter;
    connect(this, &UserManagementWidget::addUserRequested, &presenter, &UserManagementPresenter::onAddUser);
    connect(this, &UserManagementWidget::editUserRequested, &presenter, &UserManagementPresenter::onEditUser);
    connect(this, &UserManagementWidget::resetPasswordRequested, &presenter,
            &UserManagementPresenter::onResetPassword);
    connect(this, &UserManagementWidget::toggleActiveRequested, &presenter,
            &UserManagementPresenter::onToggleActive);
    connect(this, &UserManagementWidget::newUserSubmitted, &presenter,
            &UserManagementPresenter::onSubmitNewUser);
    connect(this, &UserManagementWidget::userEditSubmitted, &presenter,
            &UserManagementPresenter::onSubmitUserEdit);
    connect(this, &UserManagementWidget::passwordResetSubmitted, &presenter,
            &UserManagementPresenter::onSubmitPasswordReset);
}

void UserManagementWidget::setBusy(bool busy)
{
    m_busy = busy;
    ui->addUserButton->setEnabled(!busy);
    updateSelectionButtons();
}

void UserManagementWidget::showUsers(const QList<application::UserAccountDto> &users)
{
    // Se conserva la cuenta seleccionada al releer.
    const int selected = selectedUserId();
    m_users = users;
    const QLocale mexico(QLocale::Spanish, QLocale::Mexico);
    ui->usersTable->setRowCount(0);
    ui->usersTable->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row) {
        const application::UserAccountDto &user = users.at(row);
        QTableWidgetItem *username = cell(user.username);
        username->setData(kIdRole, user.id);
        ui->usersTable->setItem(row, UsernameColumn, username);
        ui->usersTable->setItem(row, NameColumn,
                                cell(user.isSelf ? QStringLiteral("%1 (tú)").arg(user.displayName)
                                                 : user.displayName));
        ui->usersTable->setItem(row, RoleColumn, cell(domain::displayLabel(user.role)));
        ui->usersTable->setItem(row, StatusColumn,
                                cell(user.active ? QStringLiteral("Activo") : QStringLiteral("Desactivado")));
        ui->usersTable->setItem(row, LastLoginColumn,
                                cell(user.lastLogin.isValid()
                                         ? mexico.toString(user.lastLogin, QStringLiteral("dd/MM/yyyy HH:mm"))
                                         : QStringLiteral("Nunca")));
        if (user.id == selected)
            ui->usersTable->selectRow(row);
    }
    updateSelectionButtons();
}

int UserManagementWidget::selectedUserId() const
{
    const QList<QTableWidgetItem *> selected = ui->usersTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    const QTableWidgetItem *username = ui->usersTable->item(selected.first()->row(), UsernameColumn);
    return username ? username->data(kIdRole).toInt() : -1;
}

void UserManagementWidget::updateSelectionButtons()
{
    const int id = selectedUserId();
    const application::UserAccountDto *user = nullptr;
    for (const application::UserAccountDto &candidate : std::as_const(m_users)) {
        if (candidate.id == id)
            user = &candidate;
    }
    ui->editUserButton->setEnabled(user && !m_busy);
    ui->resetPasswordButton->setEnabled(user && !m_busy);
    ui->toggleActiveButton->setText(user && !user->active ? QStringLiteral("Reactivar")
                                                          : QStringLiteral("Desactivar"));
    ui->toggleActiveButton->setEnabled(user && user->canChangeActive && !m_busy);
    ui->toggleActiveButton->setToolTip(user && !user->canChangeActive
                                           ? QStringLiteral("No puedes desactivar tu propia cuenta.")
                                           : QString());
}

void UserManagementWidget::openForm(UserAccountDialog *form)
{
    if (m_form)
        m_form->close();
    m_form = form;
    m_form->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_form, &UserAccountDialog::submitted, this, [this] {
        if (!m_form)
            return;
        switch (m_form->mode()) {
        case UserAccountDialog::Mode::Create:
            emit newUserSubmitted(m_form->newUser());
            break;
        case UserAccountDialog::Mode::Edit:
            emit userEditSubmitted(m_form->userUpdate());
            break;
        case UserAccountDialog::Mode::Password:
            emit passwordResetSubmitted(m_form->passwordReset());
            break;
        }
    });
    // open() y no exec(): modal igual, pero sin un bucle de eventos anidado
    // mientras el presenter guarda en otro hilo.
    m_form->open();
}

void UserManagementWidget::askNewUser()
{
    // Hijo de la ventana y no de esta tarjeta, como el formulario de
    // mantenimientos: nadie que busque campos por su clave dentro de la página
    // debe encontrar los del formulario.
    openForm(new UserAccountDialog(UserAccountDialog::Mode::Create, {}, window()));
}

void UserManagementWidget::askUserEdit(const application::UserAccountDto &user)
{
    openForm(new UserAccountDialog(UserAccountDialog::Mode::Edit, user, window()));
}

void UserManagementWidget::askPasswordReset(const application::UserAccountDto &user)
{
    openForm(new UserAccountDialog(UserAccountDialog::Mode::Password, user, window()));
}

void UserManagementWidget::setUserFormBusy(bool busy)
{
    if (m_form)
        m_form->setBusy(busy);
}

void UserManagementWidget::showUserFormErrors(const QList<domain::ValidationError> &errors)
{
    if (m_form)
        m_form->showErrors(errors);
}

void UserManagementWidget::closeUserForm()
{
    // Se suelta ya, no cuando se borre: un aviso que llega justo después no
    // debe salir sobre un formulario cerrado.
    if (UserAccountDialog *form = m_form.data()) {
        m_form.clear();
        form->accept();
    }
}

bool UserManagementWidget::confirmActivation(const application::UserAccountDto &user, bool activate)
{
    const QString question =
        activate ? QStringLiteral("¿Reactivar la cuenta de %1? Podrá volver a entrar.")
                 : QStringLiteral("¿Desactivar la cuenta de %1? Ya no podrá entrar, pero sus "
                                  "ventas conservan su nombre.");
    return QMessageBox::question(this, QStringLiteral("Usuarios"), question.arg(user.displayName),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

void UserManagementWidget::showMessage(const QString &message)
{
    ui->messageLabel->setText(message);
    ui->messageLabel->setVisible(!message.isEmpty());
    // Con el formulario abierto, el mensaje se vería detrás de él.
    if (m_form && !message.isEmpty())
        QMessageBox::warning(m_form, m_form->windowTitle(), message);
}

void UserManagementWidget::showNotice(const QString &notice)
{
    ui->noticeLabel->setText(notice);
    ui->noticeLabel->setVisible(!notice.isEmpty());
}
