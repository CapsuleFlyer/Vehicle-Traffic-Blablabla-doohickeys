#ifndef PARKING_H
#define PARKING_H

#include <pthread.h>
#include <semaphore.h>

#define MAX_PARKING_SPOTS 10
#define MAX_WAITING_QUEUE 5

typedef struct {
    sem_t parking_spots;
    sem_t waiting_queue;
    int current_occupancy;
    int peak_occupancy;
    int total_parked_vehicles;
    pthread_mutex_t lock;
} parking_lot_t;

parking_lot_t* parking_create(void);
void parking_destroy(parking_lot_t* lot);
int parking_wait_spot(parking_lot_t* lot);
int parking_try_enter_queue(parking_lot_t* lot);
void parking_leave_queue(parking_lot_t* lot);
void parking_leave_spot(parking_lot_t* lot);
int parking_get_occupancy(parking_lot_t* lot);
int parking_get_total_parked(parking_lot_t* lot);
int parking_get_peak(parking_lot_t* lot);

#endif