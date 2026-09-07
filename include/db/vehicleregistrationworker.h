#ifndef VEHICLEREGISTRATIONWORKER_H
#define VEHICLEREGISTRATIONWORKER_H

#include <QString>
#include <QThread>

#include <memory>

namespace domain {
class Vehicle;
}

// Corre en su propio QThread (mismo motivo que LoginWorker: una conexión
// QSqlDatabase solo puede usarse desde el hilo que la creó). Primero copia
// los archivos pendientes a storage/vehicles/{vin}/... (fuera de cualquier
// transacción SQL abierta) y luego inserta todo el vehículo de forma atómica
// vía VehicleRepository.
//
// IMPORTANTE: recibe la unidad EN PROPIEDAD, y quien lo construye debe
// pasarle una copia hecha con Vehicle::clone(), no el mismo objeto que
// conserva la vista. La razón es que este hilo REESCRIBE las rutas de los
// archivos, cambiándolas de absolutas a relativas al almacén. Si compartiera
// el objeto con la vista, un reintento tras un fallo de la base (un VIN
// duplicado, por ejemplo) intentaría copiar los archivos desde su ruta ya
// reescrita, que no existe como origen, y fallaría con un error que no dice
// nada del problema real.
class VehicleRegistrationWorker : public QThread
{
    Q_OBJECT

public:
    VehicleRegistrationWorker(std::unique_ptr<domain::Vehicle> vehicle, QString storageRoot,
                              QObject *parent = nullptr);
    ~VehicleRegistrationWorker() override;

signals:
    void registrationSucceeded(int folio);
    void registrationFailed(const QString &reason);

protected:
    void run() override;

private:
    std::unique_ptr<domain::Vehicle> m_vehicle;
    QString m_storageRoot;
};

#endif // VEHICLEREGISTRATIONWORKER_H
