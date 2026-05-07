#ifndef PARKING_H
#define PARKING_H

#include <pthread.h>
#include <semaphore.h>

#define MAX_PARKING_SPOTS 10
#define MAX_WAITING_QUEUE 5

/* Parking lot structure */
typedef struct {
    sem_t parking_spots;      /* Semaphore: max 10 cars */
    sem_t waiting_queue;      /* Semaphore: bounded waiting queue, max 5 */
    int current_occupancy;
    int total_parked_vehicles;
    pthread_mutex_t lock;
} parking_lot_t;

/* Function prototypes */
parking_lot_t* parking_create(void);
void parking_destroy(parking_lot_t* lot);
int parking_wait_spot(parking_lot_t* lot);
int parking_try_enter_queue(parking_lot_t* lot);
void parking_leave_queue(parking_lot_t* lot);
void parking_leave_spot(parking_lot_t* lot);
int parking_get_occupancy(parking_lot_t* lot);
int parking_get_total_parked(parking_lot_t* lot);

#endif /* PARKING_H */
