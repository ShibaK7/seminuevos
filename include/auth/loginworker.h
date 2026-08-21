#ifndef LOGINWORKER_H
#define LOGINWORKER_H

#include <QString>
#include <QThread>

// Corre un intento de login en su propio QThread (no en el pool compartido
// de QtConcurrent: esos hilos nunca terminan por sí solos, y sus conexiones
// se cerrarían después de destruirse QApplication). Consulta la tabla
// users y verifica la contraseña contra el hash guardado.
class LoginWorker : public QThread
{
    Q_OBJECT

public:
    LoginWorker(const QString &username, const QString &password, QObject *parent = nullptr);

signals:
    void succeeded(const QString &displayName, const QString &role);
    void failed(const QString &reason);

protected:
    void run() override;

private:
    QString m_username;
    QString m_password;
};

#endif // LOGINWORKER_H
