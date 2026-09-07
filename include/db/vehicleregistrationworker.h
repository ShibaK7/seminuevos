#ifndef VEHICLEREGISTRATIONWORKER_H
#define VEHICLEREGISTRATIONWORKER_H

#include "../vehiclewizard/vehicledraft.h"

#include <QString>
#include <QThread>

// Corre en su propio QThread (mismo motivo que LoginWorker: una conexión
// QSqlDatabase solo puede usarse desde el hilo que la creó). Primero copia
// los archivos pendientes a storage/vehicles/{vin}/... (fuera de cualquier
// transacción SQL abierta) y luego inserta todo el vehículo de forma
// atómica vía VehicleRegistrationService.
class VehicleRegistrationWorker : public QThread
{
    Q_OBJECT

public:
    VehicleRegistrationWorker(VehicleDraft draft, QString storageRoot, QObject *parent = nullptr);

signals:
    void registrationSucceeded(int folio);
    void registrationFailed(const QString &reason);

protected:
    void run() override;

private:
    VehicleDraft m_draft;
    QString m_storageRoot;
};

#endif // VEHICLEREGISTRATIONWORKER_H
