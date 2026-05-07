#include "parking.h"
#include "display.h"
#include "simulation.h" /* For global_shutdown_flag */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>

/*
 * Create and initialize a parking lot with semaphores
 */
parking_lot_t* parking_create(void) {
    parking_lot_t* lot = (parking_lot_t*)malloc(sizeof(parking_lot_t));
    if (!lot) {
        perror("malloc");
        return NULL;
    }

    /* Initialize semaphores */
    if (sem_init(&lot->parking_spots, 0, MAX_PARKING_SPOTS) == -1) {
        perror("sem_init parking_spots");
        free(lot);
        return NULL;
    }

    if (sem_init(&lot->waiting_queue, 0, MAX_WAITING_QUEUE) == -1) {
        perror("sem_init waiting_queue");
        sem_destroy(&lot->parking_spots);
        free(lot);
        return NULL;
    }

    pthread_mutex_init(&lot->lock, NULL);
    lot->current_occupancy = 0;
    lot->total_parked_vehicles = 0;

    return lot;
}

/*
 * Destroy parking lot and cleanup semaphores
 */
void parking_destroy(parking_lot_t* lot) {
    if (!lot) return;

    sem_destroy(&lot->parking_spots);
    sem_destroy(&lot->waiting_queue);
    pthread_mutex_destroy(&lot->lock);
    free(lot);
}

/*
 * Wait for a parking spot with timeout and atomic occupancy increment
 * Returns 0 on success, -1 on error or timeout
 */
int parking_wait_spot(parking_lot_t* lot) {
    if (!lot || global_shutdown_flag) return -1;
    
    /* Use short repeated tries instead of one long block */
    for (int tries = 0; tries < 5; tries++) {
        if (global_shutdown_flag) return -1;
        
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        /* ISSUE #1: Using correct dot notation for timespec members */
        ts.tv_nsec += 100000000;  /* 100ms per try */
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        
        int result = sem_timedwait(&lot->parking_spots, &ts);
        if (result == 0) {
            pthread_mutex_lock(&lot->lock);
            lot->current_occupancy++;
            lot->total_parked_vehicles++;
            pthread_mutex_unlock(&lot->lock);
            return 0;
        }
        /* ETIMEDOUT = try again. Other error = fail */
        if (errno != ETIMEDOUT) return -1;
    }
    
    /* ISSUE #3: Distinct message when timeout occurs */
    fprintf(stderr, "[PARKING] Spot TIMEOUT after 500ms - no spots available\n");
    fflush(stderr);
    return -1;  /* Gave up after 500ms total */
}

/*
 * Try to enter the waiting queue
 * Returns 0 on success, -1 if queue is full or shutdown active
 */
int parking_try_enter_queue(parking_lot_t* lot) {
    if (!lot || global_shutdown_flag) return -1;
    
    int result = sem_trywait(&lot->waiting_queue);
    
    /* ISSUE #3: Distinct message when queue is full */
    if (result == -1) {
        fprintf(stderr, "[PARKING] Queue FULL - cannot enter waiting queue\n");
        fflush(stderr);
    }
    
    return result;
}

/*
 * Leave the waiting queue (increment semaphore)
 */
void parking_leave_queue(parking_lot_t* lot) {
    if (!lot) return;
    sem_post(&lot->waiting_queue);
}

/*
 * Leave a parking spot (increment semaphore)
 */
void parking_leave_spot(parking_lot_t* lot) {
    if (!lot) return;
    
    /* ISSUE #2: Release spot BEFORE decrementing occupancy */
    /* This keeps occupancy accurate (reflects currently occupied spots) */
    sem_post(&lot->parking_spots);
    
    pthread_mutex_lock(&lot->lock);
    if (lot->current_occupancy > 0) {
        lot->current_occupancy--;
    }
    pthread_mutex_unlock(&lot->lock);
}

/*
 * Get current parking occupancy
 */
int parking_get_occupancy(parking_lot_t* lot) {
    if (!lot) return 0;
    
    pthread_mutex_lock(&lot->lock);
    int occupancy = lot->current_occupancy;
    pthread_mutex_unlock(&lot->lock);
    
    return occupancy;
}

/*
 * Get total vehicles ever parked at this lot
 */
int parking_get_total_parked(parking_lot_t* lot) {
    if (!lot) return 0;
    
    pthread_mutex_lock(&lot->lock);
    int total = lot->total_parked_vehicles;
    pthread_mutex_unlock(&lot->lock);
    
    return total;
}
