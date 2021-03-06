#include <controlit/dreamer/RobotInterfaceDreamer.hpp>

#include <chrono>
#include <controlit/Command.hpp>
#include <controlit/RTControlModel.hpp>
#include <controlit/logging/RealTimeLogging.hpp>
#include <controlit/dreamer/OdometryStateReceiverDreamer.hpp>
#include <controlit/dreamer/TimerRTAI.hpp>

#include "m3/robots/chain_name.h"
#include <m3rt/base/m3ec_def.h>
#include <m3rt/base/m3rt_def.h>

#include <rtai_shm.h>

#define TORQUE_SHM "TSHMM"
#define TORQUE_CMD_SEM "TSHMC"
#define TORQUE_STATUS_SEM "TSHMS"

namespace controlit {
namespace dreamer {

// Uncomment one of the following lines to enable/disable detailed debug statements.
#define PRINT_INFO_STATEMENT(ss)
// #define PRINT_INFO_STATEMENT(ss) CONTROLIT_INFO << ss;

#define PRINT_INFO_STATEMENT_RT(ss)
// #define PRINT_INFO_STATEMENT_RT(ss) CONTROLIT_INFO_RT << ss;

#define PRINT_INFO_RT(ss)
// #define PRINT_INFO_RT(ss) CONTROLIT_INFO_RT << ss;

#define PRINT_INFO_STATEMENT_RT_ALWAYS(ss) CONTROLIT_INFO_RT << ss;
// #define PRINT_INFO_STATEMENT_RT_ALWAYS(ss) std::cout << ss << std::endl;

#define PRINT_RECEIVED_STATE 0
#define PRINT_COMMAND 0

#define NON_REALTIME_PRIORITY 1

#define DEG_TO_RAD(deg) deg / 180 * 3.14159265359
#define RAD_TO_DEG(rad) rad / 3.14159265359 * 180

#define NUM_HAND_JOINTS 6
#define NUM_HEAD_JOINTS 7

RobotInterfaceDreamer::RobotInterfaceDreamer() :
    RobotInterface(),         // Call super-class' constructor
    sharedMemoryReady(false)
{
}

RobotInterfaceDreamer::~RobotInterfaceDreamer()
{
}

bool RobotInterfaceDreamer::init(ros::NodeHandle & nh, RTControlModel * model)
{
    PRINT_INFO_STATEMENT("Method called!");

    //---------------------------------------------------------------------------------
    // Initialize the parent class.
    //---------------------------------------------------------------------------------

    if (!RobotInterface::init(nh, model))
        return false;

    //---------------------------------------------------------------------------------
    // Initialize the hand controller.
    //---------------------------------------------------------------------------------

    handController.init(nh);
    handCommand.setZero(NUM_HAND_JOINTS);
    handJointPositions.setZero(NUM_HAND_JOINTS);
    handJointVelocities.setZero(NUM_HAND_JOINTS);

    //---------------------------------------------------------------------------------
    // Initialize the head controller.
    //---------------------------------------------------------------------------------

    headController.init(nh);
    headCommand.setZero(NUM_HEAD_JOINTS);
    headJointPositions.setZero(NUM_HEAD_JOINTS);
    headJointVelocities.setZero(NUM_HEAD_JOINTS);

    //---------------------------------------------------------------------------------
    // Create the odometry receiver.
    //---------------------------------------------------------------------------------

    PRINT_INFO_STATEMENT("Creating and initializing the odometry state receiver...");
    odometryStateReceiver.reset(new OdometryStateReceiverDreamer());
    return odometryStateReceiver->init(nh, model);
}

// This needs to be called by the RT thread provided by ServoClockDreamer.
// It is called the first time either read() or write() is called.
bool RobotInterfaceDreamer::initSM()
{
    PRINT_INFO_STATEMENT("Method called!");

    // Get a pointer to the shared memory created by the M3 Server.
    PRINT_INFO_STATEMENT("Getting point to shared memory...");
    sharedMemoryPtr = (M3Sds *) rt_shm_alloc(nam2num(TORQUE_SHM), sizeof(M3Sds), USE_VMALLOC);
    if (!sharedMemoryPtr)
    {
        CONTROLIT_ERROR << "Call to rt_shm_alloc failed for shared memory name \"" << TORQUE_SHM << "\"";
        return false;
    }

    // Get the semaphores protecting the status and command shared memory registers.
    PRINT_INFO_STATEMENT("Getting shared memory semaphores...");
    status_sem = (SEM *) rt_get_adr(nam2num(TORQUE_STATUS_SEM));
    if (!status_sem)
    {
      CONTROLIT_ERROR << "Torque status semaphore \"" << TORQUE_STATUS_SEM << "\" not found";
      return false;
    }

    command_sem = (SEM *) rt_get_adr(nam2num(TORQUE_CMD_SEM));
    if (!command_sem)
    {
      CONTROLIT_ERROR << "Torque command semaphore \"" << TORQUE_CMD_SEM << "\" not found";
      return false;
    }

    PRINT_INFO_STATEMENT("Done initializing connection to shared memory.");
    sharedMemoryReady = true;  // Prevents this method from being called again.


    return true;
}

void RobotInterfaceDreamer::printLimbSHMStatus(std::stringstream & ss, std::string prefix,
    M3TorqueShmSdsBaseStatus & shmLimbStatus)
{
    ss << prefix << " - theta: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbStatus.theta[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - thetadot: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbStatus.thetadot[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - torque: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbStatus.torque[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - wrench: [";
    for (size_t ii = 0; ii < 6; ii++)
    {
        ss << shmLimbStatus.wrench[ii];
        if (ii < 6 - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - ctrl_mode: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbStatus.ctrl_mode[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

}

void RobotInterfaceDreamer::printSHMStatus()
{
    std::stringstream ss;

    ss << "M3UTATorqueShmSdsStatus:\n"
       << " - timestamp: " << shm_status.timestamp << "\n"
       << " - right_arm:\n";

    printLimbSHMStatus(ss, "    ", shm_status.right_arm);

    ss << " - left_arm:\n";
    printLimbSHMStatus(ss, "    ", shm_status.left_arm);

    ss << " - torso:\n";
    printLimbSHMStatus(ss, "    ", shm_status.torso);

    ss << " - head:\n";
    printLimbSHMStatus(ss, "    ", shm_status.head);

    ss << " - right_hand:\n";
    printLimbSHMStatus(ss, "    ", shm_status.head);

    // NOTE: omitting mobile_base for now

    CONTROLIT_INFO << ss.str();
}

void RobotInterfaceDreamer::printLimbSHMCommand(std::stringstream & ss, std::string prefix,
    M3TorqueShmSdsBaseCommand & shmLimbCommand)
{
    ss << prefix << " - tq_desired: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbCommand.tq_desired[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - q_desired: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbCommand.q_desired[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - slew_rate_q_desired: [";
    for (size_t ii = 0; ii < MAX_NDOF; ii++)
    {
        ss << shmLimbCommand.slew_rate_q_desired[ii];
        if (ii < MAX_NDOF - 1) ss << ", ";
    }
    ss << "]\n";

    ss << prefix << " - q_stiffness: [";
    for (size_t ii = 0; ii < 6; ii++)
    {
        ss << shmLimbCommand.q_stiffness[ii];
        if (ii < 6 - 1) ss << ", ";
    }
    ss << "]\n";
}

void RobotInterfaceDreamer::printSHMCommand()
{
    std::stringstream ss;

    ss << "M3TorqueShmSdsBaseCommand:\n"
       << " - timestamp: " << shm_cmd.timestamp << "\n"
       << " - right_arm:\n";

    printLimbSHMCommand(ss, "    ", shm_cmd.right_arm);

    ss << " - left_arm:\n";
    printLimbSHMCommand(ss, "    ", shm_cmd.left_arm);

    ss << " - torso:\n";
    printLimbSHMCommand(ss, "    ", shm_cmd.torso);

    ss << " - head:\n";
    printLimbSHMCommand(ss, "    ", shm_cmd.head);

    ss << " - right_hand:\n";
    printLimbSHMCommand(ss, "    ", shm_cmd.right_hand);

    // NOTE: omitting mobile_base for now

    CONTROLIT_INFO << ss.str();
}

bool RobotInterfaceDreamer::read(controlit::RobotState & latestRobotState, bool block)
{
    //---------------------------------------------------------------------------------
    // If necessary, establish the connection to shared memory.
    //---------------------------------------------------------------------------------

    if (!sharedMemoryReady)
    {
        if (!initSM())
        {
             return false;
        }
    }

    //---------------------------------------------------------------------------------
    // Reset the timestamp within robot state to remember when the state was obtained.
    //---------------------------------------------------------------------------------

    latestRobotState.resetTimestamp();

    //---------------------------------------------------------------------------------
    // Read the latest joint state information from shared memory.
    // The information is saved into member variable shm_status.
    //---------------------------------------------------------------------------------

    PRINT_INFO_STATEMENT("Grabbing lock on status semaphore...");
    rt_sem_wait(status_sem);
    memcpy(&shm_status, sharedMemoryPtr->status, sizeof(shm_status));
    rt_sem_signal(status_sem);
    PRINT_INFO_STATEMENT("Releasing lock on status semaphore...");

    //---------------------------------------------------------------------------------
    // If the reflected sequence number is equal to the current sequence number,
    // compute the round trip communication latency and publish it.
    //---------------------------------------------------------------------------------
    if (seqno == shm_status.seqno)
    {
        double latency = rttTimer->getTime();
        publishCommLatency(latency);
    }

    // Temporary code to print everything received
    // printSHMStatus();

    //---------------------------------------------------------------------------------
    // Save the joint position data.
    //---------------------------------------------------------------------------------

    // // latestRobotState.setJointPosition(0, DEG_TO_RAD(shm_status.torso.theta[0])); // torso pan
    // // latestRobotState.setJointPosition(0, 0); // torso pan, fixed to zero since joint is not working as of 2014/10/16
    // latestRobotState.setJointPosition(0, DEG_TO_RAD(shm_status.torso.theta[1])); // torso_lower_pitch
    // latestRobotState.setJointPosition(1, DEG_TO_RAD(shm_status.torso.theta[2])); // torso_upper_pitch

    // latestRobotState.setJointPosition(2, DEG_TO_RAD(shm_status.left_arm.theta[0])); // left arm
    // latestRobotState.setJointPosition(3, DEG_TO_RAD(shm_status.left_arm.theta[1]));
    // latestRobotState.setJointPosition(4, DEG_TO_RAD(shm_status.left_arm.theta[2]));
    // latestRobotState.setJointPosition(5, DEG_TO_RAD(shm_status.left_arm.theta[3]));
    // latestRobotState.setJointPosition(6, DEG_TO_RAD(shm_status.left_arm.theta[4]));
    // latestRobotState.setJointPosition(7, DEG_TO_RAD(shm_status.left_arm.theta[5]));
    // latestRobotState.setJointPosition(8, DEG_TO_RAD(shm_status.left_arm.theta[6]));

    // latestRobotState.setJointPosition(9, DEG_TO_RAD(shm_status.head.theta[0])); // neck
    // latestRobotState.setJointPosition(10, DEG_TO_RAD(shm_status.head.theta[1]));
    // latestRobotState.setJointPosition(11, DEG_TO_RAD(shm_status.head.theta[2]));
    // latestRobotState.setJointPosition(12, DEG_TO_RAD(shm_status.head.theta[3]));

    // latestRobotState.setJointPosition(13, DEG_TO_RAD(shm_status.right_arm.theta[0])); // right arm
    // latestRobotState.setJointPosition(14, DEG_TO_RAD(shm_status.right_arm.theta[1]));
    // latestRobotState.setJointPosition(15, DEG_TO_RAD(shm_status.right_arm.theta[2]));
    // latestRobotState.setJointPosition(16, DEG_TO_RAD(shm_status.right_arm.theta[3]));
    // latestRobotState.setJointPosition(17, DEG_TO_RAD(shm_status.right_arm.theta[4]));
    // latestRobotState.setJointPosition(18, DEG_TO_RAD(shm_status.right_arm.theta[5]));
    // latestRobotState.setJointPosition(19, DEG_TO_RAD(shm_status.right_arm.theta[6]));

    // Only control the left arm, right arm, and torso pitch joints
    latestRobotState.setJointPosition(0, DEG_TO_RAD(shm_status.torso.theta[1])); // torso_lower_pitch
    latestRobotState.setJointPosition(1, DEG_TO_RAD(shm_status.torso.theta[2])); // torso_upper_pitch
    latestRobotState.setJointPosition(2, DEG_TO_RAD(shm_status.left_arm.theta[0]));
    latestRobotState.setJointPosition(3, DEG_TO_RAD(shm_status.left_arm.theta[1]));
    latestRobotState.setJointPosition(4, DEG_TO_RAD(shm_status.left_arm.theta[2]));
    latestRobotState.setJointPosition(5, DEG_TO_RAD(shm_status.left_arm.theta[3]));
    latestRobotState.setJointPosition(6, DEG_TO_RAD(shm_status.left_arm.theta[4]));
    latestRobotState.setJointPosition(7, DEG_TO_RAD(shm_status.left_arm.theta[5]));
    latestRobotState.setJointPosition(8, -1 * DEG_TO_RAD(shm_status.left_arm.theta[6]));
    latestRobotState.setJointPosition(9, DEG_TO_RAD(shm_status.right_arm.theta[0]));
    latestRobotState.setJointPosition(10, DEG_TO_RAD(shm_status.right_arm.theta[1]));
    latestRobotState.setJointPosition(11, DEG_TO_RAD(shm_status.right_arm.theta[2]));
    latestRobotState.setJointPosition(12, DEG_TO_RAD(shm_status.right_arm.theta[3]));
    latestRobotState.setJointPosition(13, DEG_TO_RAD(shm_status.right_arm.theta[4]));
    latestRobotState.setJointPosition(14, DEG_TO_RAD(shm_status.right_arm.theta[5]));
    latestRobotState.setJointPosition(15, DEG_TO_RAD(shm_status.right_arm.theta[6]));

    //---------------------------------------------------------------------------------
    // Save the joint velocity data.
    //---------------------------------------------------------------------------------

    // // latestRobotState.setJointVelocity(0, DEG_TO_RAD(shm_status.torso.thetadot[0])); // torso pan
    // // latestRobotState.setJointVelocity(0, 0); // torso pan, fixed to zero since joint is not working as of 2014/10/16
    // latestRobotState.setJointVelocity(0, DEG_TO_RAD(shm_status.torso.thetadot[1])); // torso_pitch_1
    // latestRobotState.setJointVelocity(1, DEG_TO_RAD(shm_status.torso.thetadot[2])); // torso_pitch_2

    // latestRobotState.setJointVelocity(2, DEG_TO_RAD(shm_status.left_arm.thetadot[0])); // left arm
    // latestRobotState.setJointVelocity(3, DEG_TO_RAD(shm_status.left_arm.thetadot[1]));
    // latestRobotState.setJointVelocity(4, DEG_TO_RAD(shm_status.left_arm.thetadot[2]));
    // latestRobotState.setJointVelocity(5, DEG_TO_RAD(shm_status.left_arm.thetadot[3]));
    // latestRobotState.setJointVelocity(6, DEG_TO_RAD(shm_status.left_arm.thetadot[4]));
    // latestRobotState.setJointVelocity(7, DEG_TO_RAD(shm_status.left_arm.thetadot[5]));
    // latestRobotState.setJointVelocity(8, DEG_TO_RAD(shm_status.left_arm.thetadot[6]));

    // latestRobotState.setJointVelocity(9, DEG_TO_RAD(shm_status.head.thetadot[0])); // neck
    // latestRobotState.setJointVelocity(10, DEG_TO_RAD(shm_status.head.thetadot[1]));
    // latestRobotState.setJointVelocity(11, DEG_TO_RAD(shm_status.head.thetadot[2]));
    // latestRobotState.setJointVelocity(12, DEG_TO_RAD(shm_status.head.thetadot[3]));

    // latestRobotState.setJointVelocity(13, DEG_TO_RAD(shm_status.right_arm.thetadot[0])); // right arm
    // latestRobotState.setJointVelocity(14, DEG_TO_RAD(shm_status.right_arm.thetadot[1]));
    // latestRobotState.setJointVelocity(15, DEG_TO_RAD(shm_status.right_arm.thetadot[2]));
    // latestRobotState.setJointVelocity(16, DEG_TO_RAD(shm_status.right_arm.thetadot[3]));
    // latestRobotState.setJointVelocity(17, DEG_TO_RAD(shm_status.right_arm.thetadot[4]));
    // latestRobotState.setJointVelocity(18, DEG_TO_RAD(shm_status.right_arm.thetadot[5]));
    // latestRobotState.setJointVelocity(19, DEG_TO_RAD(shm_status.right_arm.thetadot[6]));

    // Only control the left arm, right arm, and torso pitch joints
    latestRobotState.setJointVelocity(0, DEG_TO_RAD(shm_status.torso.thetadot[1])); // torso_lower_pitch
    latestRobotState.setJointVelocity(1, DEG_TO_RAD(shm_status.torso.thetadot[2])); // torso_upper_pitch
    latestRobotState.setJointVelocity(2, DEG_TO_RAD(shm_status.left_arm.thetadot[0]));
    latestRobotState.setJointVelocity(3, DEG_TO_RAD(shm_status.left_arm.thetadot[1]));
    latestRobotState.setJointVelocity(4, DEG_TO_RAD(shm_status.left_arm.thetadot[2]));
    latestRobotState.setJointVelocity(5, DEG_TO_RAD(shm_status.left_arm.thetadot[3]));
    latestRobotState.setJointVelocity(6, DEG_TO_RAD(shm_status.left_arm.thetadot[4]));
    latestRobotState.setJointVelocity(7, DEG_TO_RAD(shm_status.left_arm.thetadot[5]));
    latestRobotState.setJointVelocity(8, -1 * DEG_TO_RAD(shm_status.left_arm.thetadot[6]));
    latestRobotState.setJointVelocity(9, DEG_TO_RAD(shm_status.right_arm.thetadot[0]));
    latestRobotState.setJointVelocity(10, DEG_TO_RAD(shm_status.right_arm.thetadot[1]));
    latestRobotState.setJointVelocity(11, DEG_TO_RAD(shm_status.right_arm.thetadot[2]));
    latestRobotState.setJointVelocity(12, DEG_TO_RAD(shm_status.right_arm.thetadot[3]));
    latestRobotState.setJointVelocity(13, DEG_TO_RAD(shm_status.right_arm.thetadot[4]));
    latestRobotState.setJointVelocity(14, DEG_TO_RAD(shm_status.right_arm.thetadot[5]));
    latestRobotState.setJointVelocity(15, DEG_TO_RAD(shm_status.right_arm.thetadot[6]));

    //---------------------------------------------------------------------------------
    // Save the joint effort data.
    //---------------------------------------------------------------------------------

    // // latestRobotState.setJointEffort(0, 1.0e-3 * shm_status.torso.torque[0]); // torso pan
    // // latestRobotState.setJointEffort(0, 0); // torso pan, fixed to zero since joint is not working as of 2014/10/16
    // latestRobotState.setJointEffort(0, 1.0e-3 * shm_status.torso.torque[1]); // torso_pitch_1
    // latestRobotState.setJointEffort(1, 1.0e-3 * shm_status.torso.torque[2]); // torso_pitch_2

    // latestRobotState.setJointEffort(2, 1.0e-3 * shm_status.left_arm.torque[0]); // left arm
    // latestRobotState.setJointEffort(3, 1.0e-3 * shm_status.left_arm.torque[1]);
    // latestRobotState.setJointEffort(4, 1.0e-3 * shm_status.left_arm.torque[2]);
    // latestRobotState.setJointEffort(5, 1.0e-3 * shm_status.left_arm.torque[3]);
    // latestRobotState.setJointEffort(6, 1.0e-3 * shm_status.left_arm.torque[4]);
    // latestRobotState.setJointEffort(7, 1.0e-3 * shm_status.left_arm.torque[5]);
    // latestRobotState.setJointEffort(8, 1.0e-3 * shm_status.left_arm.torque[6]);

    // latestRobotState.setJointEffort(9, 1.0e-3 * shm_status.head.torque[0]); // neck
    // latestRobotState.setJointEffort(10, 1.0e-3 * shm_status.head.torque[1]);
    // latestRobotState.setJointEffort(11, 1.0e-3 * shm_status.head.torque[2]);
    // latestRobotState.setJointEffort(12, 1.0e-3 * shm_status.head.torque[3]);

    // latestRobotState.setJointEffort(13, 1.0e-3 * shm_status.right_arm.torque[0]); // right arm
    // latestRobotState.setJointEffort(14, 1.0e-3 * shm_status.right_arm.torque[1]);
    // latestRobotState.setJointEffort(15, 1.0e-3 * shm_status.right_arm.torque[2]);
    // latestRobotState.setJointEffort(16, 1.0e-3 * shm_status.right_arm.torque[3]);
    // latestRobotState.setJointEffort(17, 1.0e-3 * shm_status.right_arm.torque[4]);
    // latestRobotState.setJointEffort(18, 1.0e-3 * shm_status.right_arm.torque[5]);
    // latestRobotState.setJointEffort(19, 1.0e-3 * shm_status.right_arm.torque[6]);

    // Only control the left arm, right arm, and torso pitch joints
    latestRobotState.setJointEffort(0, 1.0e-3 * shm_status.torso.torque[1]); // torso_lower_pitch
    latestRobotState.setJointEffort(1, 1.0e-3 * shm_status.torso.torque[2]); // torso_upper_pitch
    latestRobotState.setJointEffort(2, 1.0e-3 * shm_status.left_arm.torque[0]);
    latestRobotState.setJointEffort(3, 1.0e-3 * shm_status.left_arm.torque[1]);
    latestRobotState.setJointEffort(4, 1.0e-3 * shm_status.left_arm.torque[2]);
    latestRobotState.setJointEffort(5, 1.0e-3 * shm_status.left_arm.torque[3]);
    latestRobotState.setJointEffort(6, 1.0e-3 * shm_status.left_arm.torque[4]);
    latestRobotState.setJointEffort(7, 1.0e-3 * shm_status.left_arm.torque[5]);
    latestRobotState.setJointEffort(8, -1.0e-3 * shm_status.left_arm.torque[6]);
    latestRobotState.setJointEffort(9, 1.0e-3 * shm_status.right_arm.torque[0]);
    latestRobotState.setJointEffort(10, 1.0e-3 * shm_status.right_arm.torque[1]);
    latestRobotState.setJointEffort(11, 1.0e-3 * shm_status.right_arm.torque[2]);
    latestRobotState.setJointEffort(12, 1.0e-3 * shm_status.right_arm.torque[3]);
    latestRobotState.setJointEffort(13, 1.0e-3 * shm_status.right_arm.torque[4]);
    latestRobotState.setJointEffort(14, 1.0e-3 * shm_status.right_arm.torque[5]);
    latestRobotState.setJointEffort(15, 1.0e-3 * shm_status.right_arm.torque[6]);

    // Get the latest hand state and update the hand controller
    for (size_t ii = 0; ii < NUM_HAND_JOINTS - 1; ii++)
    {
        handJointPositions[ii] = DEG_TO_RAD(shm_status.right_hand.theta[ii]);
        handJointVelocities[ii] = DEG_TO_RAD(shm_status.right_hand.thetadot[ii]);
    }
    handJointPositions[5] = DEG_TO_RAD(shm_status.left_hand.theta[0]);
    handJointVelocities[5] = DEG_TO_RAD(shm_status.left_hand.thetadot[0]);

    handController.updateState(handJointPositions, handJointVelocities);

    // Get the latest head joint state and update the head controller.
    for (size_t ii = 0; ii < NUM_HEAD_JOINTS; ii++)
    {
        headJointPositions[ii] = DEG_TO_RAD(shm_status.head.theta[ii]);
        headJointVelocities[ii] = DEG_TO_RAD(shm_status.head.thetadot[ii]);
    }

    headController.updateState(headJointPositions, headJointVelocities);

    //---------------------------------------------------------------------------------
    // Get and save the latest odometry data.
    //---------------------------------------------------------------------------------

    if (!odometryStateReceiver->getOdometry(latestRobotState, block))
        return false;

    //---------------------------------------------------------------------------------
    // Call the the parent class' read method.  This causes the latestrobot state
    // to be published.
    //---------------------------------------------------------------------------------

    return controlit::RobotInterface::read(latestRobotState, block);
}

bool RobotInterfaceDreamer::write(const controlit::Command & command)
{
    //---------------------------------------------------------------------------------
    // If necessary, establish the connection to shared memory.
    //---------------------------------------------------------------------------------

    if (!sharedMemoryReady)
    {
        if (!initSM())
        {
            CONTROLIT_INFO << "Shared memory failed to initialize. Aborting write.";
            return false;
        }
    }

    const Vector & cmd = command.getEffortCmd();

    //---------------------------------------------------------------------------------
    // Save the effort command into the outgoing message.
    //---------------------------------------------------------------------------------

    // // shm_cmd.torso.tq_desired[0] = 1e3 * cmd[0]; // torso pan
    // shm_cmd.torso.tq_desired[0] = 0; // torso_yaw, fixed to zero since joint is not working as of 2014/10/16
    // shm_cmd.torso.tq_desired[1] = 1e3 * cmd[0]; // torso_pitch_1
    // //shm_cmd.torso.tq_desired[2] = 1e3 * cmd[1]; // torso_pitch_2

    // shm_cmd.left_arm.tq_desired[0] = 1e3 * cmd[1]; // left arm
    // shm_cmd.left_arm.tq_desired[1] = 1e3 * cmd[2];
    // shm_cmd.left_arm.tq_desired[2] = 1e3 * cmd[3];
    // shm_cmd.left_arm.tq_desired[3] = 1e3 * cmd[4];
    // shm_cmd.left_arm.tq_desired[4] = 1e3 * cmd[5];
    // shm_cmd.left_arm.tq_desired[5] = 1e3 * cmd[6];
    // shm_cmd.left_arm.tq_desired[6] = 1e3 * cmd[7];

    // shm_cmd.head.tq_desired[0] = 1e3 * cmd[8]; // neck
    // shm_cmd.head.tq_desired[1] = 1e3 * cmd[9];
    // shm_cmd.head.tq_desired[2] = 1e3 * cmd[10];
    // shm_cmd.head.tq_desired[3] = 1e3 * cmd[11];

    // shm_cmd.right_arm.tq_desired[0] = 1e3 * cmd[12]; // right arm
    // shm_cmd.right_arm.tq_desired[1] = 1e3 * cmd[13];
    // shm_cmd.right_arm.tq_desired[2] = 1e3 * cmd[14];
    // shm_cmd.right_arm.tq_desired[3] = 1e3 * cmd[15];
    // shm_cmd.right_arm.tq_desired[4] = 1e3 * cmd[16];
    // shm_cmd.right_arm.tq_desired[5] = 1e3 * cmd[17];
    // shm_cmd.right_arm.tq_desired[6] = 1e3 * cmd[18];

    // Only control the left arm, right arm, and torso pitch joints
    shm_cmd.torso.tq_desired[0]     = 0;
    shm_cmd.torso.tq_desired[1]     = 1e3 * cmd[0]; // torso_pitch_1
    shm_cmd.torso.tq_desired[2]     = 0;            // torso_pitch_2  (slave of torso_pitch_1)
    shm_cmd.left_arm.tq_desired[0]  = 1e3 * cmd[1];
    shm_cmd.left_arm.tq_desired[1]  = 1e3 * cmd[2];
    shm_cmd.left_arm.tq_desired[2]  = 1e3 * cmd[3];
    shm_cmd.left_arm.tq_desired[3]  = 1e3 * cmd[4];
    shm_cmd.left_arm.tq_desired[4]  = 1e3 * cmd[5];
    shm_cmd.left_arm.tq_desired[5]  = 1e3 * cmd[6];
    shm_cmd.left_arm.tq_desired[6]  = -1e3 * cmd[7];
    shm_cmd.right_arm.tq_desired[0] = 1e3 * cmd[8];
    shm_cmd.right_arm.tq_desired[1] = 1e3 * cmd[9];
    shm_cmd.right_arm.tq_desired[2] = 1e3 * cmd[10];
    shm_cmd.right_arm.tq_desired[3] = 1e3 * cmd[11];
    shm_cmd.right_arm.tq_desired[4] = 1e3 * cmd[12];
    shm_cmd.right_arm.tq_desired[5] = 1e3 * cmd[13];
    shm_cmd.right_arm.tq_desired[6] = 1e3 * cmd[14];

    // Send commands to the right hand
    handController.getCommand(handCommand);

    // shm_cmd.right_hand.q_desired[0] = RAD_TO_DEG(handCommand[0]);
    // shm_cmd.right_hand.slew_rate_q_desired[0] = 10;
    // shm_cmd.right_hand.q_stiffness[0] = 1;

    for (size_t ii = 0; ii < 5; ii++)
    {
        shm_cmd.right_hand.tq_desired[ii] = 1.0e3 * handCommand[ii];
    }

    shm_cmd.left_hand.tq_desired[0] = 1.0e3 * handCommand[5]; // The left gripper accepts commands in Nm?

    // shm_cmd.right_hand.tq_desired[0] = 0;
    // shm_cmd.right_hand.tq_desired[1] = 0;
    // shm_cmd.right_hand.tq_desired[2] = 0;
    // shm_cmd.right_hand.tq_desired[3] = 0;
    // shm_cmd.right_hand.tq_desired[4] = 0;

    // shm_cmd.right_hand.q_desired[1] = 0;
    // shm_cmd.right_hand.q_desired[2] = 0;
    // shm_cmd.right_hand.q_desired[3] = 0;
    // shm_cmd.right_hand.q_desired[4] = 0;

    // shm_cmd.right_hand.q_stiffness[0] = 0;
    // shm_cmd.right_hand.q_stiffness[1] = 0;
    // shm_cmd.right_hand.q_stiffness[2] = 0;
    // shm_cmd.right_hand.q_stiffness[3] = 0;
    // shm_cmd.right_hand.q_stiffness[4] = 0;

    // Send position commands to the neck joints
    headController.getCommand(headCommand);

    // shm_cmd.head.q_desired[0] = RAD_TO_DEG(headCommand[0]);
    // shm_cmd.head.q_desired[1] = RAD_TO_DEG(headCommand[1]);
    // shm_cmd.head.q_desired[2] = RAD_TO_DEG(headCommand[2]);
    // shm_cmd.head.q_desired[3] = RAD_TO_DEG(headCommand[3]);
    // shm_cmd.head.q_desired[4] = RAD_TO_DEG(headCommand[4]);
    // shm_cmd.head.q_desired[5] = RAD_TO_DEG(headCommand[5]);
    // shm_cmd.head.q_desired[6] = RAD_TO_DEG(headCommand[6]);

    // shm_cmd.head.slew_rate_q_desired[0] = 10;
    // shm_cmd.head.slew_rate_q_desired[1] = 10;
    // shm_cmd.head.slew_rate_q_desired[2] = 10;
    // shm_cmd.head.slew_rate_q_desired[3] = 10;
    // shm_cmd.head.slew_rate_q_desired[4] = 10;
    // shm_cmd.head.slew_rate_q_desired[5] = 10;
    // shm_cmd.head.slew_rate_q_desired[6] = 10;

    //---------------------------------------------------------------------------------
    // Save the timestamp into the outgoing command message.  This is necessary for
    // the M3 Server to know a new command was received.  If it is not set, the M3
    // server will disable all joints.
    //---------------------------------------------------------------------------------

    shm_cmd.timestamp = shm_status.timestamp;

    //---------------------------------------------------------------------------------
    // If necessary, save the sequence number in the command message.  Used for
    // measuring the communication time between ControlIt! and the robot.
    //---------------------------------------------------------------------------------

    if (sendSeqno)
    {
        sendSeqno = false;
        seqno++;
        rttTimer->start();
    }

    shm_cmd.seqno = seqno;

    //---------------------------------------------------------------------------------
    // Write the command to shared memory.  This transmits the command to the M3
    // Server.
    //---------------------------------------------------------------------------------

    PRINT_INFO_STATEMENT("Getting lock on command semaphore...");
    rt_sem_wait(command_sem);
    memcpy(sharedMemoryPtr->cmd, &shm_cmd, sizeof(shm_cmd));
    rt_sem_signal(command_sem);
    PRINT_INFO_STATEMENT("Releasing lock on command semaphore...");

    //---------------------------------------------------------------------------------
    // Call the the parent class' write method.  This causes the command to be
    // published.
    //---------------------------------------------------------------------------------

    return controlit::RobotInterface::write(command);
}

std::shared_ptr<Timer> RobotInterfaceDreamer::getTimer()
{
    std::shared_ptr<Timer> timerPtr;
    timerPtr.reset(new TimerRTAI());
    return timerPtr;
}

} // namespace dreamer
} // namespace controlit
