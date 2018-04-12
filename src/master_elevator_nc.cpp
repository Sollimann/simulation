#include "ros/ros.h"
#include "std_msgs/String.h"
#include "deque"
#include "simulation/calls.h"
#include <ctime>
#include <sys/time.h>
#include <std_msgs/Float32.h>
#include <math.h>
#include <algorithm>

/********************** DEFINE ************************/

#define dt 0.1 // [s]
#define ELEVATOR_TRAVEL_SPEED 0.5 // [floor/sec]
#define ELEVATOR_DWELLTIME_AT_FLOOR 3 // [sec]
#define NUMBER_OF_FLOORS 8 // [-]
#define TOP_FLOOR 7 // [-]
#define TERMINAL_FLOOR 0 // [-]

/********************** ENUM **************************/

enum{ UP = 1, DOWN = -1}; //Call direction
enum{ E1 = 1, E2 = 2}; //ElevatorID

/***************** GLOBAL VARIABLES *******************/

float current_time;
float totTravelTimeforPassengers = 0;
float totWaitingTimeforPassengers = 0;
float totNrPassengersHandled = 0;

/************************ ELEVATOR CAR ****************************/
/*************************** CLASS ********************************/
// This class handles the elevator control of each separate elevator car

class ElevatorStatus{

public:

    //Status variables
    double exactElevatorPosition;
    unsigned int currentFloor;
    unsigned int destinationFloor;
    int travelDirection;
    bool idle;

    float totWaitingTime;
    float totTravelTime;
    float timeToNextDestinationFloor;

    //Constructor & initalizer
    ElevatorStatus();
    void initializeElevator();

    //Elevator handle algorithm
    int figureOfSuitability(int callAtFloor,int callGoingToFloor);

    //The elevator controller system
    void ElevatorStateController(int elevatorID);

    //Finds max and min floor in queue
    std::pair<unsigned int,unsigned int> getMaxAndMinFloorInQueue();

    //Elevator Ready Queues
    std::deque<std::pair<unsigned int,unsigned int> > pick_Up_Queue;
    std::deque<unsigned int> drop_Off_Queue;

};

/********************** THE MASTER ELEVATOR **************************/
/******************************* CLASS *******************************/
// This Class handles both elevators operating and includes the
// logical algorithm for choosing which elevator car that should handle
// a certain call.

class MasterElevator{

private:

    //Subscribers
    ros::Subscriber sub_elevator_call;
    ros::Subscriber sub_clock;

public:

    //Constructor
    MasterElevator(ros::NodeHandle &nh);

    //Class objects
    ElevatorStatus Elevator1;
    ElevatorStatus Elevator2;

    //Callback functions at 10Hz
    void getCurrentTime(const std_msgs::Float32::ConstPtr& subMsg);
    void pickElevatorToHandleCall(const simulation::calls& subMsg);
};

/************************ INITIALIZATION ****************************/


void ElevatorStatus::initializeElevator() {

    exactElevatorPosition = 0;
    currentFloor = 0;
    destinationFloor = 7;
    travelDirection = UP;
    idle = true;
    totWaitingTime = 0;
    totTravelTime = 0;
}

/************************* CONSTRUCTORS *****************************/

//Constructing car object
ElevatorStatus::ElevatorStatus(){
    initializeElevator();
}


//Constructing Master Elevator object
MasterElevator::MasterElevator(ros::NodeHandle &nh) {

    ElevatorStatus elev1;
    ElevatorStatus elev2;
    Elevator1 = elev1;
    Elevator2 = elev2;

    //initialize subscribers
    sub_clock = nh.subscribe("/clock",1000,&MasterElevator::getCurrentTime,this);
    sub_elevator_call = nh.subscribe("/publishCallMsg",1000,&MasterElevator::pickElevatorToHandleCall,this);
}

/************************* THE IMPLEMENTED ALGORITHM  ****************************/
/***************************** NEAREST CAR LOGIC *********************************/


int ElevatorStatus::figureOfSuitability(int callAtFloor,int callGoingToFloor) {

    //Figure of suitability
    int FS;

    int directionOfCall = callGoingToFloor - callAtFloor;
    int currentMovingDirection = destinationFloor - currentFloor;
    int distanceToCaller = callAtFloor - currentFloor;
    distanceToCaller = abs(distanceToCaller);

    //std::cout << "directionOfCall: " << directionOfCall << std::endl;
    //std::cout << "currentMovingDirection: " << currentMovingDirection << std::endl;
    //std::cout << "distanceToCaller: " << distanceToCaller << std::endl;
    //std::cout << "currentFloor: " << currentFloor << std::endl;

    if ((currentMovingDirection > 0 && directionOfCall > 0) || (currentMovingDirection < 0 && directionOfCall < 0)){
        //the elevator car is moving towards the landing call and the call is set in the same direction.
        FS = NUMBER_OF_FLOORS + 1 - (distanceToCaller-1);
        //std::cout << "FS1: ";
    }else if((currentMovingDirection < 0 && directionOfCall > 0) || (currentMovingDirection > 0 && directionOfCall < 0) ){
        //the elevator car is moving towards the landing call but the call is set to the opposite direction.
        FS = NUMBER_OF_FLOORS + 1 - distanceToCaller;
        //std::cout << "FS2: ";
    }else if(((currentFloor > callAtFloor) && currentMovingDirection > 0)||((currentFloor < callAtFloor) && currentMovingDirection < 0)){
        //the elevator car is already moving away from the landing call (the elevator is responding to some other call).
        FS = 1;
        //std::cout << "FS3: ";
    }else{
        FS = NUMBER_OF_FLOORS+1 - distanceToCaller;
        //std::cout << "FS4: ";
    }
    std::cout << std::endl;
    //std::cout << FS << std::endl;

    return FS;
}

/*********************** FIND MAX & MIN FLOOR IN QUEUES ********************/

std::pair<unsigned int,unsigned int> ElevatorStatus::getMaxAndMinFloorInQueue() {

    //if the Queues are empty
    if (pick_Up_Queue.size() == 0 && drop_Off_Queue.size() == 0){
        return std::make_pair(0,0);
    }

    //This ensures that maxFloor = minFloor if only there is one
    //element left in both queues combined
    unsigned int maxFloor = 0;
    unsigned int minFloor = 7;

    //Check pick-up Queue first if not empty
    if(pick_Up_Queue.size() > 0) {
        for (std::deque<std::pair<unsigned int, unsigned int> >::iterator it = pick_Up_Queue.begin(); it != pick_Up_Queue.end(); ++it) {
            maxFloor = std::max(it->first, maxFloor);
            minFloor = std::min(it->first, maxFloor);
        }
    }

    //Check drop-off Queue second if not empty
    if(drop_Off_Queue.size() > 0) {
        for (std::deque<unsigned int>::iterator it = drop_Off_Queue.begin(); it != drop_Off_Queue.end(); ++it) {

            maxFloor = std::max(*it, maxFloor);
            minFloor = std::min(*it, minFloor);
        }
    }

        return std::make_pair(maxFloor,minFloor);
}




/************************ ELEVATOR CONTROLLER *************************/
// This function handles the actual real-time control of the elevators
// It handles both pick-up & drop-off of passengers, sets new destinations,
// handles travelling between floors
void ElevatorStatus::ElevatorStateController(int elevatorID) {

    /*
    //Print out position
    std::cout << "Exact position of Elevator  " << elevatorID << ": " << exactElevatorPosition << std::endl;
    std::cout << "Current floor of Elevator  " << elevatorID << ": " << currentFloor << std::endl;
    std::cout << "Elevator " << elevatorID << " has " << pick_Up_Queue.size() << " waiting for pick-up" << std::endl;
    std::cout << "Elevator " << elevatorID << " has " << drop_Off_Queue.size() << " waiting for drop-off" << std::endl;
    std::cout << "Elevator " << elevatorID << " destination floor is " << destinationFloor << std::endl;
    std::cout << "Elevator " << elevatorID << " travel Direction is " << travelDirection << std::endl;
    */
        //Calculate elevator position
    //If there is any request for the elevator, then move
    //If not, stay idle
    if (pick_Up_Queue.size() > 0 || drop_Off_Queue.size() > 0) {
        idle = false;
        exactElevatorPosition += travelDirection * (ELEVATOR_TRAVEL_SPEED * dt);
        currentFloor = floor(exactElevatorPosition);


        //Check if current floor is in the pick up Queue

        if (pick_Up_Queue.size() > 0) {

            //Add up total waiting time
            //totWaitingTime += pick_Up_Queue.size() * dt;
            totWaitingTimeforPassengers += pick_Up_Queue.size() * dt;

            //std::cout << "totWaitingTime elev: " << totWaitingTime << std::endl;
            std::cout << "WAITING: " << totWaitingTimeforPassengers << std::endl;

            //Check if current floor is queued
            for (std::deque<std::pair<unsigned int, unsigned int> >::iterator it = pick_Up_Queue.begin();
                 it != pick_Up_Queue.end(); ++it) {

                //std::cout << "it->first: " << it->first << "it->second: " << it->second << std::endl;
                if (it->first == currentFloor) {
                    //If current floor is in the pick up Queue add to drop off Queue
                    //and then pop element from pick up Queue
                    drop_Off_Queue.push_back(it->second);

                    //Break out of for-loop if the queue is empty
                    if (pick_Up_Queue.size() > 0) {
                        pick_Up_Queue.erase(it);
                    }else{
                        break; //leave for-loop
                    }

                    //Also add dwelling time for stopping at floor
                    totWaitingTime += ELEVATOR_DWELLTIME_AT_FLOOR * dt;

                    std::cout << "Elevator " << elevatorID << " picks up passenger at floor: " << currentFloor
                              << std::endl;

                        break; //leave for-loop
                }//if
            }//for
        }//if

        //check if current floor is in the drop off Queue
        if (drop_Off_Queue.size() > 0) {

            //Add up total travel times
            //totTravelTime += drop_Off_Queue.size() * dt;
            totTravelTimeforPassengers += drop_Off_Queue.size() * dt;

            //std::cout << "totalTravelTime elev: " << totTravelTime << std::endl;
            std::cout << "TRAVEL: " << totTravelTimeforPassengers << std::endl;

            //Check if current floor is queued
            for (std::deque<unsigned int>::iterator it = drop_Off_Queue.begin(); it != drop_Off_Queue.end(); ++it) {

                if (*it == currentFloor) {

                    //Visit is registered by removing element from queue
                    drop_Off_Queue.erase(it);

                    std::cout << "Elevator " << elevatorID << " drops off passenger at floor: " << currentFloor
                              << std::endl;

                    //Break out of for loop if the queue is empty
                    if (drop_Off_Queue.size() == 0) {
                        break; //leave for loop
                    } //if
                } //if
            } //for


        } //if


        /******************** CHOOSING NEXT DESTINATION FLOOR *******************/
        //Here we want to set: travelDirection and destination floor

        unsigned int maxFloor = getMaxAndMinFloorInQueue().first;
        unsigned int minFloor = getMaxAndMinFloorInQueue().second;

        //std::cout << "Elevator " << elevatorID << " maxFloor is " << maxFloor << std::endl;
        //std::cout << "Elevator " << elevatorID << " minFloor is " << minFloor << std::endl;

        //Upward riding logic
        if ((maxFloor > currentFloor) && travelDirection == UP) {
            travelDirection = UP;
            destinationFloor = maxFloor;
        } else if (currentFloor == TERMINAL_FLOOR) {
            travelDirection = UP;
            destinationFloor = maxFloor;
        }

        //Downward riding logic
        if (maxFloor <= currentFloor) {
            travelDirection = DOWN;
            destinationFloor = minFloor;
        } else if ((minFloor < currentFloor) && travelDirection == DOWN) {
            travelDirection = DOWN;
            destinationFloor = minFloor;
        } else if (currentFloor == TOP_FLOOR) {
            travelDirection = DOWN;
            destinationFloor = minFloor;
        }


        //If elevator has no calls in queue
        //Then it is idle
    }else{
        idle = true;
    }

 }

/******************************* SIMULATION ********************************/
/*************************** CALLBACK FUNCTIONS ****************************/

//Updates the global timing
void MasterElevator::getCurrentTime(const std_msgs::Float32::ConstPtr& subMsg){
    current_time = subMsg->data;
}

//Handling of calls
void MasterElevator::pickElevatorToHandleCall(const simulation::calls& subMsg) {

    //New call
    bool newCall = subMsg.newCall;
    float time = subMsg.time;
    unsigned int floor = subMsg.floor;
    int direction = subMsg.direction;

    //std::cout << "Direction: " << direction << std::endl;

    if (newCall){

        //Update total number of passengers
        totNrPassengersHandled++;

        std::cout << "Passengers: " << totNrPassengersHandled << std::endl;

        //Calculate Figure of Suitability for both elevatorhs
        int FS_E1 = Elevator1.figureOfSuitability(floor,direction);
        int FS_E2 = Elevator2.figureOfSuitability(floor,direction);

        //std::cout << "FS_E1: " << FS_E1 << std::endl;
        //std::cout << "FS_E2: " << FS_E2 << std::endl;

        //make queue pair
        std::pair<unsigned int,unsigned int> call;
        call = std::make_pair(floor, direction);


        //Largest Figure of suitability get the call
        if (FS_E1 >= FS_E2){
            Elevator1.pick_Up_Queue.push_back(call);
        }else{
            Elevator2.pick_Up_Queue.push_back(call);
        }

    }//if

    //Update the state of the elevators;
    Elevator1.ElevatorStateController(E1);
    Elevator2.ElevatorStateController(E2);

    /************************** SIMULATION COMPLETES AT *****************************/

    if(current_time > 60 && Elevator1.idle && Elevator2.idle){
        std::cout << "\n \n SIMULATION IS COMPLETE! \n \n" << std::endl;
        std::cout << "STATISTICS: \n" << std::endl;

        std::cout << "Clock at completion: " << current_time << std::endl;
        std::cout << "Total number of passengers handled: " << totNrPassengersHandled << std::endl;
        std::cout << "Total waiting time for all passengers combined: " << totWaitingTimeforPassengers << std::endl;
        std::cout << "Total travel time for all passngers combined: " << totTravelTimeforPassengers << std::endl;
        std::cout << "Total response time for all passengers combined: " << (totTravelTimeforPassengers + totWaitingTimeforPassengers) << std::endl;
        std::cout << "\n \n";

        std::cout << "Average travel time: " << totTravelTimeforPassengers/totNrPassengersHandled << std::endl;
        std::cout << "Average response " << (totTravelTimeforPassengers + totWaitingTimeforPassengers)/totNrPassengersHandled << std::endl;
        std::cout << "RESULT OF OBJECTIVE FUNCTION: \n \n";
        std::cout << "Average waiting time: " << totWaitingTimeforPassengers/totNrPassengersHandled << std::endl;


        ros::shutdown();
        //to only print once
    }
}



/******************************* ROS NODEHANDLE *******************************/

int main(int argc, char** argv){
    ros::init(argc, argv, "master_elevator_nc");
    ros::NodeHandle nh;
    MasterElevator M(nh);

    ros::spin();
    return 0;
}