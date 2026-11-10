// Las reglas de un mantenimiento (#46): tipo elegido, fecha válida, un costo
// que cabe en cost NUMERIC(12, 2) y una descripción.

#include "domain/inventory/model/maintenancerecord.h"

#include <QtTest>

namespace {

domain::MaintenanceRecord withCost(const QString &cost)
{
    return {7, QDate(2026, 10, 19), 1, cost, QStringLiteral("Cambio de aceite")};
}

} // namespace

class TstMaintenanceRecord : public QObject
{
    Q_OBJECT

private slots:
    void aCompleteRecordIsValid()
    {
        const domain::MaintenanceRecord record = withCost(QStringLiteral(" 1500.50 "));
        QVERIFY(record.validate().isValid());
        // Sin los espacios: así llega al CAST de la base.
        QCOMPARE(record.cost(), QStringLiteral("1500.50"));
    }

    void theTypeMustBeChosen()
    {
        const domain::MaintenanceRecord record(7, QDate(2026, 10, 19), -1, QStringLiteral("10"),
                                               QStringLiteral("Afinación"));
        QCOMPARE(record.validate().fields(), QStringList{QStringLiteral("maintenanceType")});
    }

    void theDateMustBeValid()
    {
        const domain::MaintenanceRecord record(7, QDate(), 1, QStringLiteral("10"),
                                               QStringLiteral("Afinación"));
        QCOMPARE(record.validate().fields(), QStringList{QStringLiteral("maintenanceDate")});
    }

    void theDescriptionIsRequired_data()
    {
        QTest::addColumn<QString>("description");
        QTest::newRow("vacía") << QString();
        QTest::newRow("solo espacios") << QStringLiteral("  \n\t ");
    }
    void theDescriptionIsRequired()
    {
        QFETCH(QString, description);
        const domain::MaintenanceRecord record(7, QDate(2026, 10, 19), 1, QStringLiteral("10"),
                                               description);
        QCOMPARE(record.validate().fields(), QStringList{QStringLiteral("description")});
    }

    void costsThatFitTheColumn_data()
    {
        QTest::addColumn<QString>("cost");
        QTest::newRow("entero") << QStringLiteral("1500");
        QTest::newRow("un decimal") << QStringLiteral("1500.5");
        QTest::newRow("dos decimales") << QStringLiteral("0.99");
        QTest::newRow("cero") << QStringLiteral("0");
        QTest::newRow("tope") << QStringLiteral("9999999999.99");
        QTest::newRow("ceros a la izquierda") << QStringLiteral("00000000001500");
    }
    void costsThatFitTheColumn()
    {
        QFETCH(QString, cost);
        QVERIFY(withCost(cost).validate().isValid());
    }

    void costsThatDoNot_data()
    {
        QTest::addColumn<QString>("cost");
        QTest::newRow("vacío") << QString();
        QTest::newRow("texto") << QStringLiteral("mil");
        QTest::newRow("negativo") << QStringLiteral("-10");
        QTest::newRow("tres decimales") << QStringLiteral("10.505");
        QTest::newRow("once enteros") << QStringLiteral("10000000000");
        QTest::newRow("coma de miles") << QStringLiteral("1,500");
        QTest::newRow("punto sin decimales") << QStringLiteral("10.");
        QTest::newRow("punto sin enteros") << QStringLiteral(".5");
        QTest::newRow("con signo de pesos") << QStringLiteral("$10");
    }
    void costsThatDoNot()
    {
        QFETCH(QString, cost);
        QCOMPARE(withCost(cost).validate().fields(), QStringList{QStringLiteral("cost")});
    }
};

QTEST_APPLESS_MAIN(TstMaintenanceRecord)
#include "tst_maintenancerecord.moc"
