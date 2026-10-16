// Qué ve y qué puede hacer cada rol en el expediente de un vehículo, y el rol
// de ida y vuelta con el texto que guarda la base.

#include "domain/common/value_objects/enums.h"
#include "domain/inventory/rules/vehiclerecordaccess.h"

#include <QtTest>

class TstVehicleRecordAccess : public QObject
{
    Q_OBJECT

private slots:
    void adminCanDoEverything()
    {
        const domain::VehicleRecordAccess access(domain::UserRole::Admin);
        QVERIFY(access.canEdit());
        QVERIFY(access.canSeeDealTerms());
        QVERIFY(access.canSeeMaintenanceHistory());
    }

    // El vendedor consulta la unidad, su condición y sus archivos, pero no la
    // operación ni el historial, y no edita.
    void salesAgentOnlyReads()
    {
        const domain::VehicleRecordAccess access(domain::UserRole::SalesAgent);
        QVERIFY(!access.canEdit());
        QVERIFY(!access.canSeeDealTerms());
        QVERIFY(!access.canSeeMaintenanceHistory());
    }

    // El texto exacto del CHECK de users.role, en los dos sentidos.
    void rolesRoundTripThroughTheDatabaseText()
    {
        QCOMPARE(domain::toDbString(domain::UserRole::Admin), QStringLiteral("Administrador"));
        QCOMPARE(domain::toDbString(domain::UserRole::SalesAgent), QStringLiteral("Vendedor"));
        for (domain::UserRole role : domain::allUserRoles())
            QCOMPARE(domain::userRoleFromDb(domain::toDbString(role)), std::optional(role));
        QCOMPARE(domain::displayLabel(domain::UserRole::SalesAgent), QStringLiteral("Vendedor"));
    }

    // Un texto que no es el de la base no se adivina: ni con otras mayúsculas.
    void unknownRoleTextIsNotGuessed()
    {
        QVERIFY(!domain::userRoleFromDb(QStringLiteral("administrador")).has_value());
        QVERIFY(!domain::userRoleFromDb(QStringLiteral("Gerente")).has_value());
        QVERIFY(!domain::userRoleFromDb(QString()).has_value());
    }
};

QTEST_APPLESS_MAIN(TstVehicleRecordAccess)
#include "tst_vehiclerecordaccess.moc"
