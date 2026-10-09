#ifndef PRESENTATION_COMMON_TASKS_POOLEDTASKRUNNER_H
#define PRESENTATION_COMMON_TASKS_POOLEDTASKRUNNER_H

#include "presentation/common/tasks/taskrunner.h"

#include <QObject>
#include <QThreadPool>

namespace presentation {

// TaskRunner real: un QThreadPool propio (no el global) con pocos hilos.
//
// Propio para controlar su vida: se destruye antes que el resto de la
// aplicación y espera a que terminen las tareas en curso, así ningún hilo
// queda usando servicios ya destruidos. Además, cada hilo del pool guarda su
// conexión a la base (ConnectionPool la cachea por hilo) y, cuando el hilo
// expira por inactividad, la conexión se cierra en ese mismo hilo.
//
// Sin QtConcurrent: QThreadPool está en QtCore y alcanza.
class PooledTaskRunner final : public TaskRunner
{
public:
    explicit PooledTaskRunner(int maxThreads = 2);
    ~PooledTaskRunner() override;

protected:
    void submit(std::function<void()> work, std::function<void()> done) override;

private:
    // Vive en el hilo que creó el runner: las entregas se encolan hacia él.
    // Va declarado antes que el pool para destruirse después de él.
    QObject m_relay;
    QThreadPool m_pool;
};

} // namespace presentation

#endif // PRESENTATION_COMMON_TASKS_POOLEDTASKRUNNER_H
