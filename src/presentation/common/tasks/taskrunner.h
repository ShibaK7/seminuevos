#ifndef PRESENTATION_COMMON_TASKS_TASKRUNNER_H
#define PRESENTATION_COMMON_TASKS_TASKRUNNER_H

#include <QObject>
#include <QPointer>

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace presentation {

// Corre trabajo lento (base de datos, disco) fuera del hilo de la interfaz y
// entrega el resultado de vuelta en ese hilo. Reemplaza a los QThread que se
// creaban por cada caso de uso (LoginWorker, VehicleRegistrationWorker).
//
// run() es un Template Method: arma el envío del resultado y delega en
// submit(), que es lo único que cambia entre implementaciones. La real usa un
// pool de hilos (PooledTaskRunner); las pruebas usan una que corre todo en el
// momento, para que un presenter se pruebe sin hilos ni bucle de eventos.
//
// Reglas para quien lo usa:
//   - `work` solo puede tocar datos por valor y servicios que vivan toda la
//     aplicación: corre en otro hilo.
//   - `onDone` corre en el hilo de la interfaz y SOLO si `context` sigue vivo,
//     así que nunca encuentra un presenter ya destruido.
//   - Llamar setBusy(true) ANTES de run(): con la implementación inmediata,
//     onDone corre dentro de run().
class TaskRunner
{
public:
    virtual ~TaskRunner() = default;

    TaskRunner(const TaskRunner &) = delete;
    TaskRunner &operator=(const TaskRunner &) = delete;

    template <class Work, class Done>
    void run(QObject *context, Work work, Done onDone)
    {
        using Result = std::invoke_result_t<Work>;
        auto result = std::make_shared<std::optional<Result>>();
        QPointer<QObject> guard(context);
        submit([result, work]() mutable { result->emplace(work()); },
               [result, guard, onDone]() mutable {
                   if (guard && result->has_value())
                       onDone(**result);
               });
    }

protected:
    TaskRunner() = default;

    // Corre `work` donde corresponda y después `done` en el hilo que creó el
    // runner (el de la interfaz).
    virtual void submit(std::function<void()> work, std::function<void()> done) = 0;
};

} // namespace presentation

#endif // PRESENTATION_COMMON_TASKS_TASKRUNNER_H
