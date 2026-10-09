#ifndef TESTS_SUPPORT_FAKES_H
#define TESTS_SUPPORT_FAKES_H

// Dobles de prueba de los puertos y de las vistas. Cada uno implementa la misma
// interfaz que el adaptador o el widget real (polimorfismo), y por eso los
// casos de uso y los presenters se prueban sin base de datos, sin disco y sin
// ventanas.

#include "application/inventory/registration/ports/contractgenerator.h"
#include "application/common/ports/filestorage.h"
#include "application/inventory/ports/inventoryreader.h"
#include "application/auth/ports/passwordhasher.h"
#include "application/inventory/registration/ports/referencedatareader.h"
#include "application/auth/ports/userdirectory.h"
#include "application/inventory/registration/ports/vehiclerepository.h"
#include "domain/inventory/model/vehicle.h"
#include "presentation/inventory/iinventoryview.h"
#include "presentation/login/iloginview.h"
#include "presentation/common/tasks/taskrunner.h"

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

// Guarda las tareas y las corre cuando la prueba lo pide, en el orden que
// quiera: sirve para probar respuestas que llegan tarde o desordenadas.
class DeferredTaskRunner final : public presentation::TaskRunner
{
public:
    QList<std::function<void()>> works;
    QList<std::function<void()>> dones;

    // Corre la tarea `index` completa (trabajo y entrega).
    void finish(int index)
    {
        works.at(index)();
        dones.at(index)();
    }

protected:
    void submit(std::function<void()> work, std::function<void()> done) override
    {
        works << std::move(work);
        dones << std::move(done);
    }
};

class FakeInventoryReader final : public application::InventoryReader
{
public:
    QList<application::InventoryItemDto> items;
    QString failWith;
    QList<application::InventoryFilterDto> filters;

    QList<application::InventoryItemDto> search(const application::InventoryFilterDto &filter,
                                                QString *error) override
    {
        filters << filter;
        if (!failWith.isEmpty()) {
            if (error)
                *error = failWith;
            return {};
        }
        return items;
    }
};

class FakeInventoryView final : public presentation::IInventoryView
{
public:
    application::InventoryFilterDto currentFilter;
    QList<application::InventoryItemDto> shown;
    QStringList messages;
    QList<int> scrolledTo;

    application::InventoryFilterDto filter() const override { return currentFilter; }
    void showVehicles(const QList<application::InventoryItemDto> &vehicles) override { shown = vehicles; }
    void showInventoryMessage(const QString &message) override { messages << message; }
    void scrollToFolio(int folio) override { scrolledTo << folio; }
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

class InMemoryVehicleRepository final : public application::VehicleRepository
{
public:
    QStringList existingSerialNumbers;
    bool failNextAdd = false;
    bool duplicateOnAdd = false;
    int adds = 0;
    int nextFolio = 100;

    bool serialNumberExists(const QString &serialNumber, QString *) override
    {
        return existingSerialNumbers.contains(serialNumber);
    }
    SaveOutcome add(domain::Vehicle &vehicle) override
    {
        ++adds;
        SaveOutcome outcome;
        if (duplicateOnAdd) {
            outcome.duplicateSerialNumber = true;
            outcome.errorMessage = QStringLiteral("duplicate key vehicles_serial_number_key");
            return outcome;
        }
        if (failNextAdd) {
            outcome.errorMessage = QStringLiteral("server closed the connection");
            return outcome;
        }
        outcome.ok = true;
        outcome.folio = nextFolio++;
        vehicle.assignFolio(outcome.folio);
        return outcome;
    }
};

// Almacén falso: anota lo que se guarda y lo que se borra. failAtStore = n
// hace fallar la n-ésima copia (1 = la primera).
class FakeFileStorage final : public application::FileStorage
{
public:
    QStringList stored;
    QStringList removed;
    int failAtStore = 0;

    Stored store(const QString &vehicleKey, const QString &sourcePath, Kind) override
    {
        Stored result;
        if (failAtStore > 0 && stored.size() + 1 == failAtStore) {
            result.errorMessage = QStringLiteral("disco lleno");
            return result;
        }
        result.ok = true;
        result.relativePath = QStringLiteral("vehicles/%1/%2").arg(vehicleKey, sourcePath);
        stored << result.relativePath;
        return result;
    }
    bool remove(const QString &relativePath) override
    {
        removed << relativePath;
        return true;
    }
    QString absolutePath(const QString &relativePath) const override { return relativePath; }

    // Lo que inspect() responde por ruta. Sin entrada, el archivo "existe",
    // se abre y su contenido es PNG.
    QMap<QString, domain::FileFacts> facts;
    QString temporaryError; // si no está vacío, copyToTemporary() falla así
    QStringList temporaryCopies;

    domain::FileFacts inspect(const QString &sourcePath) const override
    {
        if (facts.contains(sourcePath))
            return facts.value(sourcePath);
        domain::FileFacts file;
        file.fileName = sourcePath.section(QLatin1Char('/'), -1);
        file.suffix = file.fileName.section(QLatin1Char('.'), -1);
        file.isReadableFile = true;
        file.size = 100;
        file.opened = true;
        file.contentTypes << QStringLiteral("image/png");
        return file;
    }
    QByteArray read(const QString &sourcePath) const override { return sourcePath.toUtf8(); }
    QByteArray readThumbnail(const QString &imagePath, int, int) const override
    {
        return QByteArray("miniatura:") + imagePath.toUtf8();
    }
    application::TemporaryFileDto copyToTemporary(const QString &sourcePath,
                                                  const QString &fileName) override
    {
        application::TemporaryFileDto result;
        if (!temporaryError.isEmpty()) {
            result.errorMessage = temporaryError;
            return result;
        }
        temporaryCopies << sourcePath;
        result.ok = true;
        result.path = QStringLiteral("/tmp/") + fileName;
        return result;
    }
};

class FakeReferenceDataReader final : public application::ReferenceDataReader
{
public:
    std::optional<double> uma = 117.31;

    QList<application::CatalogOptionDto> vehicleCategories(QString *) override
    {
        return {{1, QStringLiteral("Automóvil"), -1}, {5, QStringLiteral("Sedán"), 1}};
    }
    QList<application::CatalogOptionDto> brands(QString *) override
    {
        return {{3, QStringLiteral("Nissan"), -1}};
    }
    QList<application::CatalogOptionDto> fuelTypes(QString *) override
    {
        return {{1, QStringLiteral("Gasolina"), -1}};
    }
    QList<application::ChecklistItemDto> conditionChecklist(QString *) override
    {
        return {{7, QStringLiteral("Exterior"), QStringLiteral("Llantas")}};
    }
    std::optional<double> umaDailyValue(QString *) override { return uma; }
};

class FakeContractGenerator final : public application::ContractGenerator
{
public:
    QList<domain::ContractData> generated;
    Outcome generate(const domain::ContractData &contract, const QString &) override
    {
        generated << contract;
        return {true, QString()};
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
