/*
 * The Priority Task Scheduler
 * SKELETON IMPLEMENTATION TO BE FILLED IN FOR TASK 1
 */

#include <infos/kernel/sched.h>
#include <infos/kernel/thread.h>
#include <infos/kernel/log.h>
#include <infos/util/list.h>
#include <infos/util/lock.h>
#include <infos/util/map.h>

using namespace infos::kernel;
using namespace infos::util;

/**
 * A Multiple Queue priority scheduling algorithm
 */
class MultipleQueuePriorityScheduler : public SchedulingAlgorithm
{
public:
    /**
     * Returns the friendly name of the algorithm, for debugging and selection purposes.
     */
    const char* name() const override { return "mq"; }

    /**
     * Called during scheduler initialisation.
     */
    void init()
    {
        List<SchedulingEntity *> real_entities;
        List<SchedulingEntity *> interactive_entities;
        List<SchedulingEntity *> normal_entities;
        List<SchedulingEntity *> daemon_entities;
        runqueues.add(SchedulingEntityPriority::REALTIME, real_entities);
        runqueues.add(SchedulingEntityPriority::INTERACTIVE, interactive_entities);
        runqueues.add(SchedulingEntityPriority::NORMAL, normal_entities);
        runqueues.add(SchedulingEntityPriority::DAEMON, daemon_entities);
    }

    /**
     * Called when a scheduling entity becomes eligible for running.
     * @param entity
     */
    void add_to_runqueue(SchedulingEntity& entity) override
    {
        UniqueIRQLock l;
        List<SchedulingEntity *> runqueue;

        switch (entity.priority())
        {
            case SchedulingEntityPriority::REALTIME:
                if (runqueues.try_get_value(SchedulingEntityPriority::REALTIME, runqueue))
                {
                    runqueue.enqueue(&entity);

                    runqueues.remove(SchedulingEntityPriority::REALTIME);
                    runqueues.add(SchedulingEntityPriority::REALTIME, runqueue);
                }
                else
                {
                    syslog.messagef(LogLevel::DEBUG, "Missing runqueue");
                }
                break;
            
            case SchedulingEntityPriority::INTERACTIVE:
                if (runqueues.try_get_value(SchedulingEntityPriority::INTERACTIVE, runqueue))
                {
                    runqueue.enqueue(&entity);

                    runqueues.remove(SchedulingEntityPriority::INTERACTIVE);
                    runqueues.add(SchedulingEntityPriority::INTERACTIVE, runqueue);
                }
                else
                {
                    syslog.messagef(LogLevel::DEBUG, "Missing runqueue");
                }
                break;

            case SchedulingEntityPriority::NORMAL:
                if (runqueues.try_get_value(SchedulingEntityPriority::NORMAL, runqueue))
                {
                    runqueue.enqueue(&entity);

                    runqueues.remove(SchedulingEntityPriority::NORMAL);
                    runqueues.add(SchedulingEntityPriority::NORMAL, runqueue);
                }
                else
                {
                    syslog.messagef(LogLevel::DEBUG, "Missing runqueue");
                }
                break;

            case SchedulingEntityPriority::DAEMON:
                if (runqueues.try_get_value(SchedulingEntityPriority::DAEMON, runqueue))
                {
                    runqueue.enqueue(&entity);

                    runqueues.remove(SchedulingEntityPriority::DAEMON);
                    runqueues.add(SchedulingEntityPriority::DAEMON, runqueue);
                }
                else
                {
                    syslog.messagef(LogLevel::DEBUG, "Missing runqueue");
                }
                break;
            
            default:
                syslog.messagef(LogLevel::DEBUG, "Thread priority unknown ?");
                break;
        }
    }

    /**
     * Called when a scheduling entity is no longer eligible for running.
     * @param entity
     */
    void remove_from_runqueue(SchedulingEntity& entity) override
    {
        UniqueIRQLock l;
        List<SchedulingEntity *> runqueue;
        unsigned int pre;

        for (int i = 0; i < 4; i++)
        {
            if (runqueues.try_get_value(i, runqueue))
            {
                pre = runqueue.count();
                runqueue.remove(&entity);
                if (runqueue.count() < pre) 
                { 
                    runqueues.remove(i);
                    runqueues.add(i, runqueue);
                    return; 
                }
            }
            else
            {
                syslog.messagef(LogLevel::DEBUG, "Missing runqueue");
            }
        }
    }

    /**
     * Called every time a scheduling event occurs, to cause the next eligible entity
     * to be chosen.  The next eligible entity might actually be the same entity, if
     * e.g. its timeslice has not expired.
     */
    SchedulingEntity *pick_next_entity() override
    {
        List<SchedulingEntity *> runqueue;

        for (int i = 0; i < 4; i++)
        {
            if (runqueues.try_get_value(i, runqueue))
            {
                if (runqueue.empty())
                {
                    if (i < 4) { continue; }
                    else { return NULL; }
                }
                if (runqueue.count() == 1) { return runqueue.first(); }

                SchedulingEntity *next = runqueue.pop();
                runqueue.enqueue(next);

                runqueues.remove(i);
                runqueues.add(i, runqueue);

                return next;
            }
            else
            {
                syslog.messagef(LogLevel::DEBUG, "Missing runqueue");
            }
        }
        return NULL;
    }

private:
    Map<int, List<SchedulingEntity *>> runqueues;
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

RegisterScheduler(MultipleQueuePriorityScheduler);