#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "parking.h"
#include "vehicle.h"

#define LIGHT_RED 0
#define LIGHT_GREEN 1

typedef struct {
    int state;
    time_t last_change;
} traffic_light_t;

typedef struct {
    int id;
    pthread_mutex_t intersection_lock;
    traffic_light_t north_light;
    traffic_light_t south_light;
    traffic_light_t east_light;
    traffic_light_t west_light;
    parking_lot_t* parking_lot;
    int emergency_vehicle_present;
    int vehicles_crossing;
    int cycle_duration;
    int green_duration;
} intersection_t;

intersection_t* intersection_create(int id, int cycle_duration, int green_duration);
void intersection_destroy(intersection_t* intersection);
void intersection_lock(intersection_t* intersection);
void intersection_unlock(intersection_t* intersection);
void intersection_set_emergency_mode(intersection_t* intersection, int enable);
void intersection_update_lights(intersection_t* intersection);
void intersection_add_crossing_vehicle(intersection_t* intersection);
void intersection_remove_crossing_vehicle(intersection_t* intersection);
int intersection_can_cross(intersection_t* intersection, int is_emergency);
int intersection_get_crossing_count(intersection_t* intersection);

#endif