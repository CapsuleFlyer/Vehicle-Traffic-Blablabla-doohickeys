#ifndef VEHICLE_H
#define VEHICLE_H

#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_VEHICLES 15
#define VEHICLE_TYPE_LEN 32
#define LOCATION_LEN 20

#define PRIORITY_EMERGENCY 3 
#define PRIORITY_MEDIUM 2    
#define PRIORITY_NORMAL 1    

typedef enum {
    STRAIGHT,
    LEFT_TURN,
    RIGHT_TURN
} direction_t;

typedef enum {
    SPAWNED,
    WAITING_AT_ENTRANCE,
    WAITING_FOR_SIGNAL,
    WAITING_FOR_PARKING,
    CROSSING,
    PARKED,
    LEFT_INTERSECTION
} vehicle_state_t;

typedef struct {
    int id;
    char type[VEHICLE_TYPE_LEN];
    char origin[LOCATION_LEN];
    direction_t destination;
    int priority;
    time_t arrival_time;
    vehicle_state_t current_state;
    int intersection_id;  
    pthread_t thread_id;
    int is_active;
} vehicle_t;

void* vehicle_thread_func(void* arg);
vehicle_t* create_vehicle(int id, const char* type, int priority, int intersection_id);
void destroy_vehicle(vehicle_t* vehicle);
const char* get_vehicle_type_color(const char* type);
const char* get_state_string(vehicle_state_t state);

#endif 
