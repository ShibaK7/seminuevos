// Pruebas del TaskRunner real: el trabajo corre fuera del hilo principal, la
// entrega vuelve al hilo principal, y un contexto ya destruido no recibe nada.

#include "presentation/tasks/pooledtaskrunner.h"

#include <QAtomicInt>
#include <QThread>
#include <QtTest>

class TstPooledTaskRunner : public QObject
{
    Q_OBJECT

private slots:
    void workRunsOffTheMainThreadAndDoneOnIt()
    {
        presentation::PooledTaskRunner runner;
        QObject context;
        QThread *mainThread = QThread::currentThread();
        bool delivered = false;
        QThread *deliveredOn = nullptr;

        runner.run(&context,
                   [] { return QThread::currentThread(); },
                   [&](QThread *workThread) {
                       delivered = true;
                       deliveredOn = QThread::currentThread();
                       QVERIFY(workThread != mainThread);
                   });

        QTRY_VERIFY(delivered);
        QCOMPARE(deliveredOn, mainThread);
    }

    void resultArrivesIntact()
    {
        presentation::PooledTaskRunner runner;
        QObject context;
        QString received;
        runner.run(&context, [] { return QStringLiteral("folio 42"); },
                   [&](const QString &value) { received = value; });
        QTRY_COMPARE(received, QStringLiteral("folio 42"));
    }

    // Si el presenter se destruyó mientras su tarea corría, la entrega se
    // descarta en vez de tocar un objeto que ya no existe.
    void destroyedContextReceivesNothing()
    {
        presentation::PooledTaskRunner runner;
        QAtomicInt finished(0);
        bool delivered = false;
        {
            auto *context = new QObject;
            runner.run(context,
                       [&finished] {
                           QThread::msleep(50);
                           finished.storeRelease(1);
                           return 1;
                       },
                       [&](int) { delivered = true; });
            delete context;
        }
        QTRY_VERIFY(finished.loadAcquire() == 1);
        QTest::qWait(100);
        QVERIFY(!delivered);
    }
};

QTEST_GUILESS_MAIN(TstPooledTaskRunner)
#include "tst_pooledtaskrunner.moc"
