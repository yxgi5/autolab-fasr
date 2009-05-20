#ifndef fasrrobot_H
#define fasrrobot_H

#include "RapiLooseStage"
#include "logwriter.h"
#include "broadcast.h"
#include "charger.h"
#include "destination.h"
#include "nd.h"
#include "worktaskrobotinterface.h"
#include "robotwaitinterface.h"
#include "wavefrontmap.h"
#include "stagegridmap.h"
#include <list>

//#define HAVE_GRIPPER 0

/** Cruise speed for normal driving [m/s] */
const float CRUISE_SPEED = 0.4;
/** Speed while avoiding obstacles [m/s] */
const float AVOID_SPEED = 0.05;
/** Turn rate for avoiding obstracles [rad/s] */
const float AVOID_TURN = 0.5;
/** Speed for precise navigation while docking [m/s] */
const float DOCK_SPEED = 0.1;
/** Minimal frontal distance to obstacles [m] */
const float MIN_FRONT_DISTANCE = 0.7;
/** Distance for a hard stop [m] */
const float STOP_DISTANCE = 0.5;
/** Duration we keep the same turn direction before reconsidering [steps] */
const int AVOID_DURATION = 10;
/** Gripper distance  [m] */
const float GRIPPER_DISTANCE = 0.2;
/** Distance to back off [m] */
const float BACK_OFF_DISTANCE = 0.7;
/** Speed used to back off from a charger [m/s] */
const float BACK_OFF_SPEED = -0.05;
/** Time out for Nd stall situations [s] */
const float ND_STALL_TIMEOUT = 10.0;
/** Distance to loading bay that is considered close enough [m] */
const float LOADING_BAY_DISTANCE = 0.3;
/** Time out for approaching pickup or delivery zone [s] */
const float LOADINGZONE_APPROACH_TIMEOUT = 30.0;
/** Distance limit for loading zone [m] */
const float LOADINGZONE_DISTANCE_LIMIT = 0.3;
/** Time out to detect that we did not make any progress [s] */
const float NO_PROGRESS_TIMEOUT = 20.0;
/** Speed used while trying to recover from a stall [m/s] */
const float STALL_RECOVER_SPEED = 0.1;
/** Turn rate while trying to recover from a stall [rad/s] */
const float STALL_RECOVER_TURNRATE = 0.5;
/**
 * Length of a sensor field box, an obstacle with in this box triggers
 * wavefront replanning [m]
 */
const float REPLANNING_BOX_LENGTH = 3.0;
/**
 * Length of a sensor field box, an obstacle with in this box triggers
 * wavefront replanning [m]
 */
const float REPLANNING_BOX_WIDTH = 0.5;
/** Energy needed to travel 1 meter [Wh] */
const float ENERGY_PER_METER = 0.11;

using namespace Rapi;

/**
 * This class provides a generic FASR robot
 * @author Jens Wawerla
 */
class ABaseRobotCtrl : public ARobotCtrl, public IRobotWaitInterface
{
  public:
    /** Default destructor */
    ~ABaseRobotCtrl();
    /**
     * Adds a task to the list of work tasks, the list is sorted by reward rate
     * @param task to be added
     */
    virtual void addWorkTask ( IWorkTaskRobotInterface* task );
    /**
     * Removes a task from the list of work tasks
     * @param task to be removed
     */
    virtual void removeWorkTask ( IWorkTaskRobotInterface* task );
    /**
     * Adds a charger to the list of available chargers
     * @param charger to be added
     */
    void addCharger ( CCharger* charger );
    /**
     * Sets the depot, a place for unallocated robot to go to
     * @param depot
     */
    void setDepot ( CDestination* depot );
    /**
     * Sets the broadcast sytem
     * @param broadcast
     */
    void setBroadCast ( CBroadCast* broadCast );
    /**
     * Gets the name of the robot controller
     * @return name of robot controller
     */
    char* getName();
    /**
     * Gets the curren position of the robot
     * @return robot position
     */
    CPose2d getPosition() { return mRobotPose; };

  protected:
    /**
     * Default constructor
     * @param robot controlled by this controller
     */
    ABaseRobotCtrl ( ARobot* robot );
    /**
     * Chooses the next work task according to some policy
     * @param dt length of time step [s]
     */
    virtual void selectWorkTaskPolicy ( float dt ) = 0;
    /**
     * Policy for start up
     * @param dt length of time step [s]
     */
    virtual void startupPolicy ( float dt ) = 0;
    /**
     * Policy for leaving a charger
     * @param dt length of time step [s]
     */
    virtual void leaveChargerPolicy ( float dt ) = 0;
    /**
     * Policy for waiting at source
     * @param dt length of time step [s]
     */
    virtual void waitAtSourcePolicy ( float dt ) = 0;
    /**
     * Policy for waiting at charger
     * @param dt length of time step [s]
     */
    virtual void waitAtChargerPolicy ( float dt ) = 0;
    /**
     * Policy for the first unallocated robot
     * @param dt length of time step [s]
     */
    virtual void unallocatedPolicy ( float dt ) = 0;
    /**
     * Policy for a successfull pick up
     * @param dt length of time step [s]
     */
    virtual void pickupCompletedPolicy ( float dt ) = 0;
    /**
     * Policy for waiting at the pickup patch, note not in the queue
     * @param dt length of time step [s]
     */
    virtual void waitingAtPickupPolicy ( float dt ) = 0;

    /** Type definition for charger data structure */
    typedef struct {
      /** Bearing of charging station [rad] */
      float bearing;
      /** Distance to charging station [m] */
      float distance;
      /** Orientation of charging station [rad] */
      float orientation;
    } tChargingStation;
    /** Type definition for FSM */
    typedef enum {START, PAUSE, GOTO_SINK, GOTO_SOURCE, GOTO_CHARGER, DOCK,
                  UNDOCK, CHARGE, PICKUP_LOAD, DELIVER_LOAD, WAITING_AT_SOURCE,
                  WAITING_AT_SINK, WAITING_AT_CHARGER, RECOVER, GOTO_DEPOT, UNALLOCATED,
                  NUM_STATES
               } tFsmState;

    /**
     * Updates the controller for one time step
     * @param dt time since last update [s]
     */
    void updateData ( float dt );
    /**
     * An action that causes the robot to dock at a charging station
     * @return true if docking was successfull, false if not docked yet
     */
    bool actionDock();
    /**
     * Action causes the robot to undock from a charging station
     * @return true is undock completed, false if still undocking
     */
    bool actionUnDock();
    /**
     * Avoids obstacles if one is detected
     * @return 1 of avoiding, 0 otherwise
     */
    int actionAvoidObstacle();
    /**
     * This action makes the robot drop off an item at the source,
     * given that we are at the source
     * @return true if pick up completed, false if still in progress
     */
    bool actionDeliverLoad();
    /**
     * This action makes the robot pick up an item at the sink,
     * given that we are at the sink
     * @return true if pick up completed, false if still in progress
     */
    bool actionPickupLoad();
    /**
     * This action makes the robot wait in the waiting queue
     * @param fgEnforceWaitingPosition forces the robot to alway visit any
     *        waiting position, even if there is nobody to wait for. This can be
     *        usefull if we need to enforce a precise heading when leaving the waiting queue
     * @return true if done waiting flase if still waiting
     */
    bool actionWaitInQueue ( bool fgEnforceWaitingPosition = false );
    /**
     * Causes the robot to drive a long a path planned and given by the
     * waypoint list
     * @return flags if at final desination (true), otherwise false
     */
    bool actionFollowWayPointList();
    /**
     * Tries to recover from a no progress situation by doing something random
     * @return true if action completed, false otherwise
     */
    bool actionRecover();
    /**
     * Checks if the robot is in a queue and if so causes it to leave the queue
     */
    void leaveCurrentWaitingQueue();
    /**
     * Checks if we made any progress
     * @param timeout [s]
     * @return true if no progress was made in the last timeout seconds, false otherwise
     */
    bool noProgress ( float timeout );
    /**
     * Make a decision about recharging, it only makes sense to go charging
     * if at the sink, therefore this method gets called after a puck delivery
     * @param dt length of time step [s]
     * @return return true if charging needed, false otherwise
     */
    virtual bool chargingPolicy ( float dt );
    /**
     * Replans the wavefront map and optains a new waypoint list
     * to make it to the goal
     * @param goal to navigate to
     * @param force enforces are replan even if the method thinks it is not required
     */
    void replan ( const CPoint2d goal, const bool force = false );
    /** Prints statistics about the task allocation to stdout */
    void printTaskStatistics();
    /**
     * Powers the robot all all devices up or down, power down will save energy
     * @param on true power up, false power down
     */
    void powerUp ( bool on );

    /** Stage position model */
    CLooseStageDrivetrain2dof* mDrivetrain;
    /** Stage laser model */
    CLooseStageLaser* mLaser;
    /** Stage power pack */
    CLooseStagePowerPack* mPowerPack;
    /** Stage fiducial model */
    CLooseStageFiducialFinder* mFiducial;
    /** Text display */
    CLooseStageTextDisplay* mTextDisplay;
    /** List of work tasks */
    std::vector<IWorkTaskRobotInterface*> mWorkTaskVector;
    /** Current work task */
    IWorkTaskRobotInterface* mCurrentWorkTask;
    /** Flags if a charging station was detected or not, using fiducial */
    bool mFgChargerDetected;
    /** Flags if a robot was detected or not, using fiducial*/
    bool mFgRobotDetected;
    /** Information about last seen charging station */
    tChargingStation mLastChargingStation;
    /** State machine of robot controller */
    tFsmState mState;
    /** Previous state of FSM */
    tFsmState mPrevState;
    /** State of FSM from previous time step*/
    tFsmState mPrevTimestepState;
    /** List of charger location */
    std::list<CCharger*> mChargerDestinationList;
    /** Current charger destination */
    CCharger* mCurrentChargerDestination;
    /** Near-Distance obstacle avoidance */
    CNd* mNd;
    /** Robot pose */
    CPose2d mRobotPose;
    /** Velocity of robot */
    CVelocity2d mRobotVelocity;
    /** ID of robot */
    int mRobotId;
    /** Cargo bay capacity of robot [#flags] */
    int mCargoBayCapacity;
    /** Current level of cargo load [#flags] */
    int mCargoLoad;
    /** Overall rewards obtained */
    float mTotalReward;
    /** Time spend traveling between sink and source without waiting [s] */
    float mTravelDuration;
    /** Time stamp of start of trip between sink and source */
    float mTravelStartTimeStamp;
    /** Time needed to complete the pick up [s] */
    float mPickupDuration;
    /**
     * Flags wether the state of the state machine has changed since the last
     * time step or not
     */
    bool mFgStateChanged;
    /** Flags if the waypoint list has changed */
    bool mFgWayPointListChanged;
    /** List of way points */
    std::list<CWaypoint2d> mWayPointList;
    /** Elapsed time since the current FSM state was entered */
    float mElapsedStateTime;
    /** Wavefront map for general purpose */
    CWaveFrontMap* mWaveFrontMap;
    /** Time it took to complete the last work task [s] */
    float mTaskCompletionTime;
    /** Simulation time [s] */
    double mSimTime;
    /** Rate of reward form the last successfully completed task [1/s] */
    float mRewardRate;
    /** Broadcast system */
    CBroadCast* mBroadCast;
    /** Waiting queue rank */
    int mQueueRank;
    /** Counts the number of completed tasks */
    int mCountTasksCompleted;
    /**
     * Flags if the reward rate from the current task is published or not
     * e.g. charging lowers the rate, so if can be useful not to tell the
     * other about it, because it will lower the average
     */
    bool mFgPublishTaskInformation;

  private:
    /**
     * Transfers waypoints to stage for drawing
     */
    void transferWaypointToStage();
    /** Update the fidicual stuff */
    int fiducialUpdate ();
    /** Pose of the current way point */
    CWaypoint2d mCurrentWaypoint;
    /** Count of avoidance time steps [steps] */
    int mAvoidCount;
    /** Destination to wait for */
    CDestination* mWaitDestination;
    /** Location of depot */
    CDestination* mDepotDestination;
    /** General purpose timer, only to be used by the current action [s] */
    float mTimer;
    /** Timer for progress monitoring [s] */
    float mProgressTimer;
    /** Single word describtion of FSM states */
    char mFsmText[20][NUM_STATES];
    /** Stores a pose to measure progress */
    CPose2d mProgressPose;
    /** Heading for recovery [rad] */
    float mHeading;
    /** Recovery time out [s] */
    float mRecoveryTimeout;
    /** Speed command for stall recovery */
    CVelocity2d mStallRecoverSpeedCmd;
    /** Marks the simulation time when the last work task was started [s] */
    float mWorkTaskStartedTimeStamp;
    /** Reward obtained from the last task */
    float mReward;
    /** Flags if we are working or doing some otherthing like charging */
    bool mFgWorking;
    /** Log file */
    CLogWriter* mLogWriter;
    /** Amount of energy dissipated [Wh] */
    float mTotalEnergyDissipated;
    /** Flags if subscribed to stage models */
    bool mFgPoweredUp;
    /** Task id for loging purposes */
    int mTaskId;
    /** Time stamp of last stall event [s] */
    float mLastStalledTimestamp;
    CTimer* mQueueStallRecoverTimer;
};


#endif
