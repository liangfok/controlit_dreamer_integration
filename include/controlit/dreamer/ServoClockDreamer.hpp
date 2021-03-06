#ifndef __CONTROLIT_DREAMER_INTEGRATION_SERVO_CLOCK_DREAMER_RTAI_HPP__
#define __CONTROLIT_DREAMER_INTEGRATION_SERVO_CLOCK_DREAMER_RTAI_HPP__

#include <controlit/ServoClock.hpp>
#include <thread>  // for std::mutex

#include <rtai_sched.h>
#include <rtai_shm.h>
#include <rtai.h>
#include <rtai_sem.h>
#include <rtai_nam2num.h>
#include <rtai_registry.h>

namespace controlit {
namespace dreamer {

typedef enum {
    RT_THREAD_UNDEF,
    RT_THREAD_INIT,
    RT_THREAD_RUNNING,
    RT_THREAD_CLEANUP,
    RT_THREAD_ERROR,
    RT_THREAD_DONE
} rt_thread_state_t;

/*!
 * The coordinator for robots that are controlled via ROS topics.
 *
 * The robot_hardware_interface::ImpedanceCommandInterface type is
 * used as the template type for the Coordinator parent class but is
 * not actually used by this class.
 */
class ServoClockDreamer : public controlit::ServoClock
{
public:
    /*!
     * The constructor.
     */
    ServoClockDreamer();

    /*!
     * The destructor.
     */
    virtual ~ServoClockDreamer();

    /*!
     * This is executed by the real-time thread.
     */
    void * rtMethod(void * arg);

protected:

    /*!
     * The implementation of the update loop.
     */
    virtual void updateLoopImpl();

private:

    /*!
     * The current state of the real-time thread.
     */
    rt_thread_state_t rtThreadState;

    /*!
     * The period of the real-time servo loop in nanoseconds.
     */
    long long rtPeriod_ns;
};

} // namespace dreamer
} // namespace controlit

#endif // __CONTROLIT_DREAMER_INTEGRATION_SERVO_CLOCK_DREAMER_RTAI_HPP__
