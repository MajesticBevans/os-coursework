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
class AdvancedMultipleQueuePriorityScheduler : public SchedulingAlgorithm
{
public:
    /**
     * Returns the friendly name of the algorithm, for debugging and selection purposes.
     */
    const char* name() const override { return "adv"; }

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
        bool looped = false;

        for (int i = 0; i < priority_levels_count; i++)
        {
            // skip priority level if max consecutive slices reached
            if (consecutive_counts[i] >= consecutive_maxs[i])
            {
                continue;
            }
            runqueue = &runqueues[i];

            if (runqueue->empty())
            {
                // if not lowest priority, reset consecutive and continue
                if (i < SchedulingEntityPriority::DAEMON) 
                { 
                    consecutive_counts[i] = 0;
                    continue; 
                }
                else 
                { 
                    // looped ensures that if there is a thread to be run, it is run
                    if (looped) { return NULL; }
                    else
                    {
                        reset_consecutive_counts();
                        i = -1;
                        looped = true;
                        continue;
                    }
                }
            }

            if (runqueue->count() == 1) 
            { 
                consecutive_counts[i]++;
                // reset all if all consecutive counts maxed out
                if (consecutive_counts[SchedulingEntityPriority::DAEMON] >= consecutive_maxs[SchedulingEntityPriority::DAEMON]) 
                { 
                    reset_consecutive_counts();
                }
                return runqueue->first(); 
            }

            //cfs algorithm
            SchedulingEntity::EntityRuntime min_runtime = 0;
            SchedulingEntity *min_runtime_entity = NULL;

            for (const auto& entity : *runqueue) {
                if (min_runtime_entity == NULL || entity->cpu_runtime() < min_runtime) {
                    min_runtime_entity = entity;
                    min_runtime = entity->cpu_runtime();
                }
            }
            consecutive_counts[i]++;

            // reset all if all consecutive counts maxed out
            if (consecutive_counts[SchedulingEntityPriority::DAEMON] >= consecutive_maxs[SchedulingEntityPriority::DAEMON]) 
            { 
                reset_consecutive_counts();
            }

            return min_runtime_entity;
        }
        return NULL;
    }

private:
    const int priority_levels_count = 4;
    List<SchedulingEntity *> runqueues[4];
    int consecutive_counts[4] = {0,0,0,0};
    int consecutive_maxs[4] = {4,3,2,1};
    
    void reset_consecutive_counts()
    {
        consecutive_counts[0] = 0;
        consecutive_counts[1] = 0;
        consecutive_counts[2] = 0;
        consecutive_counts[3] = 0;
    }
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

RegisterScheduler(AdvancedMultipleQueuePriorityScheduler);