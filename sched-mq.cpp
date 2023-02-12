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
        // Meh?
    }

    /**
     * Called when a scheduling entity becomes eligible for running.
     * @param entity
     */
    void add_to_runqueue(SchedulingEntity& entity) override
    {
        UniqueIRQLock l;
        switch (entity.priority())
        {
            case SchedulingEntityPriority::REALTIME:
                runqueues[SchedulingEntityPriority::REALTIME].enqueue(&entity);
                break;
            
            case SchedulingEntityPriority::INTERACTIVE:
                runqueues[SchedulingEntityPriority::INTERACTIVE].enqueue(&entity);
                break;

            case SchedulingEntityPriority::NORMAL:
                runqueues[SchedulingEntityPriority::NORMAL].enqueue(&entity);
                break;

            case SchedulingEntityPriority::DAEMON:
                runqueues[SchedulingEntityPriority::DAEMON].enqueue(&entity);
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
        List<SchedulingEntity *> * runqueue;
        unsigned int pre;

        for (int i = 0; i < 4; i++)
        {
            runqueue = &runqueues[i];
           
            pre = runqueue->count();
            runqueue->remove(&entity);
            if (runqueue->count() < pre) { return; }
        }
    }

    /**
     * Called every time a scheduling event occurs, to cause the next eligible entity
     * to be chosen.  The next eligible entity might actually be the same entity, if
     * e.g. its timeslice has not expired.
     */
    SchedulingEntity *pick_next_entity() override
    {
        List<SchedulingEntity *> * runqueue;

        for (int i = 0; i < 4; i++)
        {
            runqueue = &runqueues[i];

            if (runqueue->empty())
            {
                if (i < 4) { continue; }
                else { return NULL; }
            }

            if (runqueue->count() == 1) { return runqueue->first(); }

            SchedulingEntity *next = runqueue->pop();
            runqueue->enqueue(next);
            syslog.messagef(LogLevel::DEBUG, "Runqueue top and bottom: %d -> %d", runqueue->first(), runqueue->last());
            return next;
        }
        return NULL;
    }

private:
    List<SchedulingEntity *> runqueues[4];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

RegisterScheduler(MultipleQueuePriorityScheduler);