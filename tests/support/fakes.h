#ifndef TESTS_SUPPORT_FAKES_H
#define TESTS_SUPPORT_FAKES_H

// Dobles de prueba de los puertos y de las vistas. Cada uno implementa la misma
// interfaz que el adaptador o el widget real (polimorfismo), y por eso los
// casos de uso y los presenters se prueban sin base de datos, sin disco y sin
// ventanas.

#include "application/ports/passwordhasher.h"
#include "application/ports/userdirectory.h"
#include "presentation/presenters/iloginview.h"
#include "presentation/tasks/taskrunner.h"

#include <QMap>
#include <QStringList>

namespace fakes {

// Corre el trabajo y la entrega en el momento, en el mismo hilo.
class InlineTaskRunner final : public presentation::TaskRunner
{
public:
    int submitted = 0;

protected:
    void submit(std::function<void()> work, std::function<void()> done) override
    {
        ++submitted;
        work();
        done();
    }
};

// Hash instantáneo y legible: "plain:<contraseña>".
class FakePasswordHasher final : public application::PasswordHasher
{
public:
    QString hash(const QString &plainPassword) const override
    {
        return QStringLiteral("plain:") + plainPassword;
    }
    bool verify(const QString &plainPassword, const QString &storedHash) const override
    {
        return storedHash == hash(plainPassword);
    }
};

class InMemoryUserDirectory final : public application::UserDirectory
{
public:
    QMap<QString, application::UserRecordDto> users;
    QString failWith; // si no está vacío, la consulta "falla" con este error
    int lookups = 0;

    std::optional<application::UserRecordDto> findByUsername(const QString &username,
                                                             QString *error) override
    {
        ++lookups;
        if (!failWith.isEmpty()) {
            if (error)
                *error = failWith;
            return std::nullopt;
        }
        if (!users.contains(username))
            return std::nullopt;
        return users.value(username);
    }
};

class FakeLoginView final : public presentation::ILoginView
{
public:
    QList<bool> busyChanges;
    QStringList errors;

    void setBusy(bool busy) override { busyChanges.append(busy); }
    void showLoginError(const QString &message) override { errors.append(message); }
};

} // namespace fakes

#endif // TESTS_SUPPORT_FAKES_H
