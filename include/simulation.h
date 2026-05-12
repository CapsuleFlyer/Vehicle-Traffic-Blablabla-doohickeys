#ifndef SIMULATION_H
#define SIMULATION_H

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "intersection.h"
#include "parking.h"
#include "vehicle.h"
#include "ipc.h"

typedef struct {
    intersection_t* f10_intersection;
    intersection_t* f11_intersection;
    vehicle_t** vehicles;
    int vehicle_count;
    pipe_pair_t* ipc_pipes;
    int f10_controller_pid;
    int f11_controller_pid;
    volatile int shutdown_flag;
    volatile int spawn_active;
    int total_parked_vehicles;
    pthread_mutex_t parked_count_lock;
} simulation_t;

extern simulation_t* global_simulation;
extern volatile int global_shutdown_flag;

void signal_handler(int sig);
void shutdown_simulation(void);
void setup_signal_handlers(void);
void spawn_vehicle_threads(simulation_t* sim);

#endif
