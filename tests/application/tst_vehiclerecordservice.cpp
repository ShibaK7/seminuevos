// Caso de uso del expediente de un vehículo (US-03.3 y US-03.4) con puertos
// falsos: qué deja ver cada rol.

#include "application/inventory/registration/services/vehiclerecordservice.h"

#include <QtTest>

class TstVehicleRecordService : public QObject
{
    Q_OBJECT

private slots:
    // La decisión es del dominio (VehicleRecordAccess); el servicio solo la
    // entrega como DTO, que es lo que la pantalla sí puede leer.
    void accessFollowsTheRole()
    {
        const application::VehicleRecordService service;

        const application::RecordAccessDto admin = service.accessFor(domain::UserRole::Admin);
        QVERIFY(admin.canEdit);
        QVERIFY(admin.showsDealTerms);
        QVERIFY(admin.showsMaintenanceHistory);

        const application::RecordAccessDto seller = service.accessFor(domain::UserRole::SalesAgent);
        QVERIFY(!seller.canEdit);
        QVERIFY(!seller.showsDealTerms);
        QVERIFY(!seller.showsMaintenanceHistory);
    }
};

QTEST_APPLESS_MAIN(TstVehicleRecordService)
#include "tst_vehiclerecordservice.moc"
