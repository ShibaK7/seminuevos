#ifndef PRESENTATION_SETTINGS_SETTINGSVIEW_H
#define PRESENTATION_SETTINGS_SETTINGSVIEW_H

#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class SettingsView;
}
QT_END_NAMESPACE

class CashBalancesWidget;
class SystemSettingsWidget;
class UserManagementWidget;

namespace presentation {
class CashBalancesPresenter;
class SystemSettingsPresenter;
class UserManagementPresenter;
}

// La página Configuraciones (US-07.1), solo para el Administrador: los
// parámetros del sistema, los saldos (Caja Chica, Cantidad Banco y Caja
// Fuerte) y los usuarios. Su formulario es settingsview.ui, en esta misma
// carpeta: las tres tarjetas van promovidas dentro de un área con scroll.
//
// Solo junta las tres secciones: cada una tiene su vista y su presenter.
class SettingsView : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsView(QWidget *parent = nullptr);
    ~SettingsView() override;

    SystemSettingsWidget &systemSettings();
    CashBalancesWidget &cashBalances();
    UserManagementWidget &userManagement();

    // Conecta cada sección con su presenter. Sin esto, confirmLeave() deja
    // salir y refresh() no hace nada (así basta una página sin presenters en
    // las pruebas de la cáscara).
    void bind(presentation::SystemSettingsPresenter &parameters,
              presentation::CashBalancesPresenter &balances,
              presentation::UserManagementPresenter &users);

    // Vuelve a leer las tres secciones (al entrar a la página), sin pisar
    // parámetros a media edición.
    void refresh();
    // Si se puede salir: pregunta si hay parámetros sin guardar, y espera a
    // que termine de guardarse un movimiento.
    bool confirmLeave();

private:
    Ui::SettingsView *ui;
    QPointer<presentation::SystemSettingsPresenter> m_parameters;
    QPointer<presentation::CashBalancesPresenter> m_balances;
    QPointer<presentation::UserManagementPresenter> m_users;
};

#endif // PRESENTATION_SETTINGS_SETTINGSVIEW_H
