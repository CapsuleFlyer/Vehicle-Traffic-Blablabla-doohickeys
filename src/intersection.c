#include "intersection.h"
#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

intersection_t* intersection_create(int id, int cycle_duration, int green_duration) {
    intersection_t* intersection = (intersection_t*)malloc(sizeof(intersection_t));
    if (!intersection) {
        perror("malloc");
        return NULL;
    }

    intersection->id = id;
    intersection->cycle_duration = cycle_duration;
    intersection->green_duration = green_duration;
    intersection->emergency_vehicle_present = 0;
    intersection->vehicles_crossing = 0;

    intersection->north_light.state = LIGHT_RED;
    intersection->north_light.last_change = time(NULL);
    
    intersection->south_light.state = LIGHT_RED;
    intersection->south_light.last_change = time(NULL);
    
    intersection->east_light.state = LIGHT_GREEN;
    intersection->east_light.last_change = time(NULL);
    
    intersection->west_light.state = LIGHT_GREEN;
    intersection->west_light.last_change = time(NULL);

    pthread_mutex_init(&intersection->intersection_lock, NULL);

    intersection->parking_lot = parking_create();
    if (!intersection->parking_lot) {
        free(intersection);
        return NULL;
    }

    return intersection;
}

void intersection_destroy(intersection_t* intersection) {
    if (!intersection) return;

    if (intersection->parking_lot) {
        parking_destroy(intersection->parking_lot);
    }

    pthread_mutex_destroy(&intersection->intersection_lock);
    
    free(intersection);
}

void intersection_lock(intersection_t* intersection) {
    if (intersection) {
        pthread_mutex_lock(&intersection->intersection_lock);
    }
}

void intersection_unlock(intersection_t* intersection) {
    if (intersection) {
        pthread_mutex_unlock(&intersection->intersection_lock);
    }
}

void intersection_set_emergency_mode(intersection_t* intersection, int enable) {
    if (!intersection) return;

    pthread_mutex_lock(&intersection->intersection_lock);
    intersection->emergency_vehicle_present = enable;
    
    if (enable) {
        intersection->north_light.state = LIGHT_GREEN;
        intersection->south_light.state = LIGHT_GREEN;
        intersection->east_light.state = LIGHT_RED;
        intersection->west_light.state = LIGHT_RED;
        
        fprintf(stderr,
            "[F%d] EMERGENCY MODE ON - N/S GREEN, E/W RED\n",
            10 + intersection->id);
        fflush(stderr);
    } else {
        intersection->north_light.state = LIGHT_RED;
        intersection->south_light.state = LIGHT_RED;
        intersection->east_light.state = LIGHT_GREEN;
        intersection->west_light.state = LIGHT_GREEN;
        
        intersection->north_light.last_change = time(NULL);
        intersection->east_light.last_change = time(NULL);
        
        fprintf(stderr,
            "[F%d] EMERGENCY MODE OFF - resuming normal\n",
            10 + intersection->id);
        fflush(stderr);
    }
    
    pthread_mutex_unlock(&intersection->intersection_lock);
}

void intersection_update_lights(intersection_t* intersection) {
    if (!intersection) return;

    pthread_mutex_lock(&intersection->intersection_lock);
    
    if (intersection->emergency_vehicle_present) {
        pthread_mutex_unlock(&intersection->intersection_lock);
        return;
    }
    
    time_t now = time(NULL);
    
    int cycle_seconds = intersection->green_duration / 1000;
    if ((now - intersection->north_light.last_change) >= cycle_seconds) {
        
        intersection->north_light.state = 
            (intersection->north_light.state == LIGHT_GREEN)
            ? LIGHT_RED : LIGHT_GREEN;
        intersection->south_light.state = 
            intersection->north_light.state;
        
        intersection->east_light.state = 
            (intersection->north_light.state == LIGHT_GREEN)
            ? LIGHT_RED : LIGHT_GREEN;
        intersection->west_light.state = 
            intersection->east_light.state;
        
        intersection->north_light.last_change = now;
        intersection->east_light.last_change = now;
        
        fprintf(stderr, 
            "[F%d] Light changed: N/S=%s E/W=%s\n",
            10 + intersection->id,
            intersection->north_light.state == LIGHT_GREEN
            ? "GREEN" : "RED",
            intersection->east_light.state == LIGHT_GREEN
            ? "GREEN" : "RED");
        fflush(stderr);
    }
    
    pthread_mutex_unlock(&intersection->intersection_lock);
}

void intersection_add_crossing_vehicle(intersection_t* intersection) {
    if (!intersection) return;
    pthread_mutex_lock(&intersection->intersection_lock);
    intersection->vehicles_crossing++;
    pthread_mutex_unlock(&intersection->intersection_lock);
}

void intersection_remove_crossing_vehicle(intersection_t* intersection) {
    if (!intersection) return;
    pthread_mutex_lock(&intersection->intersection_lock);
    if (intersection->vehicles_crossing > 0) {
        intersection->vehicles_crossing--;
    }
    pthread_mutex_unlock(&intersection->intersection_lock);
}

int intersection_can_cross(intersection_t* intersection, int is_emergency) {
    if (!intersection) return 0;
    
    if (is_emergency) return 1;
    
    pthread_mutex_lock(&intersection->intersection_lock);
    
    if (intersection->emergency_vehicle_present) {
        pthread_mutex_unlock(&intersection->intersection_lock);
        return 0;
    }
    
    int crossing = intersection->vehicles_crossing;
    int can_cross = (crossing < 2) ? 1 : 0;
    
    pthread_mutex_unlock(&intersection->intersection_lock);
    return can_cross;
}

int intersection_get_crossing_count(intersection_t* intersection) {
    if (!intersection) return 0;
    pthread_mutex_lock(&intersection->intersection_lock);
    int count = intersection->vehicles_crossing;
    pthread_mutex_unlock(&intersection->intersection_lock);
    return count;
}