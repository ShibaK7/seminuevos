#ifndef VEHICLEREPOSITORY_H
#define VEHICLEREPOSITORY_H

#include <QString>
#include <QList>
#include <QDateTime>

class VehicleRepository {
public:
    VehicleRepository() = delete;

    static bool getVehicleById(int id, VehicleDTO& outDto);

    static bool save(VehicleDTO& dto, int& outId);
};

#endif // VEHICLEREPOSITORY_H