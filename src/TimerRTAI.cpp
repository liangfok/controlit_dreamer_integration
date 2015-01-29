#include <controlit/dreamer/TimerRTAI.hpp>


namespace controlit {
namespace dreamer {

TimerRTAI::TimerRTAI()
{
}

void TimerRTAI::start()
{
    startTime = rt_get_cpu_time_ns();;
}

double TimerRTAI::getTime()
{
    return (rt_get_cpu_time_ns() - startTime) / 1e6;
}

} // namespace dreamer
} // namespace controlit
