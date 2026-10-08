#include "presentation/tasks/pooledtaskrunner.h"

#include <QMetaObject>

namespace presentation {

PooledTaskRunner::PooledTaskRunner(int maxThreads)
{
    m_pool.setMaxThreadCount(maxThreads);
}

PooledTaskRunner::~PooledTaskRunner()
{
    // Espera las tareas en curso. Lo que ya se haya encolado hacia m_relay se
    // descarta solo cuando m_relay se destruye: un QObject borra sus eventos
    // pendientes.
    m_pool.waitForDone();
}

void PooledTaskRunner::submit(std::function<void()> work, std::function<void()> done)
{
    QObject *relay = &m_relay;
    m_pool.start([work = std::move(work), done = std::move(done), relay]() {
        work();
        QMetaObject::invokeMethod(relay, done, Qt::QueuedConnection);
    });
}

} // namespace presentation
