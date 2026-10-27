#ifndef PRESENTATION_COMMERCIAL_COMMERCIALPRESENTER_H
#define PRESENTATION_COMMERCIAL_COMMERCIALPRESENTER_H

#include "application/commercial/services/commercialservice.h"

#include <QObject>

#include <atomic>
#include <memory>

namespace presentation {
class ICommercialView;
class TaskRunner;

class CommercialPresenter final : public QObject
{
public:
    CommercialPresenter(ICommercialView &view, const application::CommercialService &service,
                        TaskRunner &runner, QObject *parent = nullptr);

public:
    void reload();

private:
    ICommercialView &m_view;
    const application::CommercialService &m_service;
    TaskRunner &m_runner;
    std::shared_ptr<std::atomic<int>> m_generation =
        std::make_shared<std::atomic<int>>(0);
};

} // namespace presentation

#endif // PRESENTATION_COMMERCIAL_COMMERCIALPRESENTER_H
