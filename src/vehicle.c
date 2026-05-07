#define _POSIX_C_SOURCE 200809L

#include "vehicle.h"
#include "intersection.h"
#include "display.h"
#include "simulation.h"
#include "graphics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

/* Macro to safely exit thread on shutdown signal */
#define CHECK_SHUTDOWN_AND_EXIT() \
    do { \
        if (global_shutdown_flag) { \
            if (is_emergency) { \
                intersection_set_emergency_mode(intersection, 0); \
            } \
            vehicle->is_active = 0; \
            pthread_exit(NULL); \
        } \
    } while(0)

/*
 * Create a vehicle with given parameters
 */
vehicle_t* create_vehicle(int id, const char* type, int priority, int intersection_id) {
    vehicle_t* vehicle = (vehicle_t*)malloc(sizeof(vehicle_t));
    if (!vehicle) {
        perror("malloc");
        return NULL;
    }

    /* CRITICAL: Zero initialize entire structure to prevent garbage data */
    memset(vehicle, 0, sizeof(vehicle_t));
    
    vehicle->id = id;
    vehicle->priority = priority;
    vehicle->intersection_id = intersection_id;
    vehicle->arrival_time = time(NULL);
    vehicle->current_state = SPAWNED;
    vehicle->is_active = 1;
    vehicle->thread_id = 0;

    /* Safely copy vehicle type string */
    if (type && strlen(type) > 0) {
        strncpy(vehicle->type, type, VEHICLE_TYPE_LEN - 1);
        vehicle->type[VEHICLE_TYPE_LEN - 1] = '\0';
    } else {
        strncpy(vehicle->type, "Unknown", VEHICLE_TYPE_LEN - 1);
        vehicle->type[VEHICLE_TYPE_LEN - 1] = '\0';
    }

    /* Assign random origin - with proper bounds checking */
    const char* origins[] = {"North", "South", "East", "West"};
    int origin_idx = rand() % (sizeof(origins) / sizeof(origins[0]));
    strncpy(vehicle->origin, origins[origin_idx], LOCATION_LEN - 1);
    vehicle->origin[LOCATION_LEN - 1] = '\0';
    
    /* Assign random destination direction */
    int dir_idx = rand() % 3;
    if (dir_idx == 0) vehicle->destination = STRAIGHT;
    else if (dir_idx == 1) vehicle->destination = LEFT_TURN;
    else vehicle->destination = RIGHT_TURN;

    return vehicle;
}

/*
 * Destroy a vehicle
 */
void destroy_vehicle(vehicle_t* vehicle) {
    if (vehicle) {
        free(vehicle);
    }
}

/*
 * Get color based on vehicle type
 */
const char* get_vehicle_type_color(const char* type) {
    if (!type) return COLOR_WHITE;
    
    if (strcmp(type, "Ambulance") == 0 || strcmp(type, "Firetruck") == 0) {
        return COLOR_RED;
    } else if (strcmp(type, "Bus") == 0) {
        return COLOR_YELLOW;
    } else if (strcmp(type, "Car") == 0) {
        return COLOR_GREEN;
    } else if (strcmp(type, "Bike") == 0) {
        return COLOR_CYAN;
    } else if (strcmp(type, "Tractor") == 0) {
        return COLOR_WHITE;
    }
    return COLOR_WHITE;
}

/*
 * Get string representation of vehicle state
 */
const char* get_state_string(vehicle_state_t state) {
    switch (state) {
        case SPAWNED:
            return "Spawned";
        case WAITING_AT_ENTRANCE:
            return "Waiting at Entrance";
        case WAITING_FOR_SIGNAL:
            return "Waiting for Signal";
        case WAITING_FOR_PARKING:
            return "Waiting for Parking";
        case CROSSING:
            return "Crossing";
        case PARKED:
            return "Parked";
        case LEFT_INTERSECTION:
            return "Left";
        default:
            return "Unknown";
    }
}

/*
 * Vehicle thread function - simulates a vehicle's journey through intersection
 * 
 * EMERGENCY vehicles: skip parking, go straight to crossing with all lights green
 * BUS vehicles: always attempt parking, then cross
 * NORMAL vehicles: 50% chance to park, then cross
 */
void* vehicle_thread_func(void* arg) {
    vehicle_t* vehicle = (vehicle_t*)arg;
    if (!vehicle || !global_simulation) {
        /* CRITICAL: Mark inactive even on error - prevents hang */
        if (vehicle) vehicle->is_active = 0;
        pthread_exit(NULL);
    }

    intersection_t* intersection = (vehicle->intersection_id == 0) ? 
        global_simulation->f10_intersection : 
        global_simulation->f11_intersection;

    if (!intersection) {
        /* CRITICAL: Mark inactive before exit - prevents hang */
        vehicle->is_active = 0;
        pthread_exit(NULL);
    }

    int is_emergency = (vehicle->priority == PRIORITY_EMERGENCY);
    int is_medium = (vehicle->priority == PRIORITY_MEDIUM);
    char log_msg[256];

    /* ===== STEP 1: LOG ARRIVAL WITH TIMESTAMP ===== */
    char time_buf[32];
    struct tm* tm_info = localtime(&vehicle->arrival_time);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    fprintf(stderr, "[%s #%d] Arrival time: %s from %s\n",
            vehicle->type, vehicle->id, time_buf, vehicle->origin);

    /* ===== STEP 2: SET STATE TO WAITING_AT_ENTRANCE ===== */
    vehicle->current_state = WAITING_AT_ENTRANCE;

    /* ===== STEP 3: PRINT ARRIVAL MESSAGE ===== */
    memset(log_msg, 0, sizeof(log_msg));
    snprintf(log_msg, sizeof(log_msg) - 1, "[%s #%d] Arrived at F%d from %s", 
             vehicle->type, vehicle->id, 10 + vehicle->intersection_id, vehicle->origin);
    display_print_vehicle_log(5, log_msg, get_vehicle_type_color(vehicle->type));
    print_vehicle_status(vehicle->arrival_time, vehicle->type, vehicle->id,
                        vehicle->origin, "Intersection", vehicle->priority, "ARRIVED");
    graphics_log_event(vehicle->type, "ARRIVED", vehicle->intersection_id, vehicle->id);

    /* ===== PRIORITY-BASED WAIT TIME ===== */
    if (is_emergency) {
        fprintf(stderr, "[%s #%d] EMERGENCY VEHICLE - NO WAIT\n", vehicle->type, vehicle->id);
        for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
            usleep(1000 / 5);
        if (global_shutdown_flag) {
            vehicle->is_active = 0;
            pthread_exit(NULL);
        }
    } else if (is_medium) {
        fprintf(stderr, "[Bus #%d] Medium priority - reduced wait time (0.1 sec)\n", vehicle->id);
        for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
            usleep(50000 / 5);
        if (global_shutdown_flag) {
            vehicle->is_active = 0;
            pthread_exit(NULL);
        }
    } else {
        fprintf(stderr, "[%s #%d] Normal priority - standard wait time (0.3 sec)\n", 
                vehicle->type, vehicle->id);
        for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
            usleep(50000 / 5);
        if (global_shutdown_flag) {
            vehicle->is_active = 0;
            pthread_exit(NULL);
        }
    }
    CHECK_SHUTDOWN_AND_EXIT();

    /* ===== EMERGENCY FLOW ===== */
    if (is_emergency) {
        fprintf(stderr, "[%s #%d] EMERGENCY! Sending alert and setting emergency mode\n",
                vehicle->type, vehicle->id);
        
        print_emergency_alert(vehicle->type, 
                            (vehicle->intersection_id == 0) ? "F10" : "F11",
                            (vehicle->intersection_id == 0) ? "F11" : "F10");
        
        /* STEP 4: SEND EMERGENCY ALERT VIA PIPE */
        if (global_simulation->ipc_pipes) {
            ipc_send_emergency_alert(global_simulation->ipc_pipes, 
                                    vehicle->intersection_id, vehicle);
        }
        
        /* STEP 5: SET EMERGENCY MODE ON INTERSECTION */
        /* NOTE: intersection_set_emergency_mode() acquires lock internally */
        intersection_set_emergency_mode(intersection, 1);
        
        /* STEP 6-7: ADD TO CROSSING */
        intersection_add_crossing_vehicle(intersection);
        
        /* STEP 8: PRINT EMERGENCY CROSSING MESSAGE ===== */
        memset(log_msg, 0, sizeof(log_msg));
        snprintf(log_msg, sizeof(log_msg) - 1, "[%s #%d] EMERGENCY CROSSING F%d - ALL LIGHTS GREEN", 
                 vehicle->type, vehicle->id, 10 + vehicle->intersection_id);
        display_print_vehicle_log(5, log_msg, get_vehicle_type_color(vehicle->type));
        print_vehicle_status(vehicle->arrival_time, vehicle->type, vehicle->id,
                            vehicle->origin, "Intersection", vehicle->priority, "EMERGENCY_CROSSING");
        graphics_log_event(vehicle->type, "EMERGENCY", vehicle->intersection_id, vehicle->id);
        
        /* STEP 9: SLEEP FOR CROSSING TIME IN 100MS CHUNKS ===== */
        int emergency_chunks = 2 + (rand() % 3);  /* 2-5 chunks = 200-500ms */
        for (int c = 0; c < emergency_chunks; c++) {
            for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
                usleep(100000 / 5);
            if (global_shutdown_flag) {
                vehicle->is_active = 0;
                pthread_exit(NULL);
            }
        }
        CHECK_SHUTDOWN_AND_EXIT();
        
        /* STEP 10: REMOVE FROM CROSSING ===== */
        intersection_remove_crossing_vehicle(intersection);
        
        /* STEP 11: RESET EMERGENCY MODE ===== */
        intersection_set_emergency_mode(intersection, 0);
        
        /* STEP 12: PRINT LEFT INTERSECTION ===== */
        fprintf(stderr, "[%s #%d] Left intersection after emergency crossing\n",
                vehicle->type, vehicle->id);
        
        /* STEP 13-15: CLEANUP AND EXIT ===== */
        vehicle->current_state = LEFT_INTERSECTION;
        vehicle->is_active = 0;
        pthread_exit(NULL);
    }

    /* ===== NON-EMERGENCY FLOW ===== */
    
    /* DETERMINE IF SHOULD PARK */
    int should_park = 0;
    if (is_medium) {
        should_park = 1;  /* Bus always attempts parking */
        fprintf(stderr, "[Bus #%d] Attempting to park (Bus always tries)\n", vehicle->id);
    } else {
        should_park = (rand() % 2 == 0);  /* 50% chance for normal vehicles */
        if (should_park) {
            fprintf(stderr, "[%s #%d] Will attempt parking (50%% luck)\n", 
                    vehicle->type, vehicle->id);
        }
    }

    /* ===== PARKING SECTION ===== */
    if (should_park) {
        vehicle->current_state = WAITING_FOR_PARKING;
        
        /* STEP 9: TRY ENTER WAITING QUEUE */
        if (parking_try_enter_queue(intersection->parking_lot) == 0) {
            fprintf(stderr, "[%s #%d] In queue - NOT blocking intersection\n",
                    vehicle->type, vehicle->id);
            
            /* STEP 10: WAIT FOR PARKING SPOT WITH TIMEOUT */
            if (parking_wait_spot(intersection->parking_lot) == 0) {
                vehicle->current_state = PARKED;
                
                /* BUG #3: Lock before incrementing total_parked_vehicles */
                pthread_mutex_lock(&global_simulation->parked_count_lock);
                global_simulation->total_parked_vehicles++;
                pthread_mutex_unlock(&global_simulation->parked_count_lock);
                
                memset(log_msg, 0, sizeof(log_msg));
                snprintf(log_msg, sizeof(log_msg) - 1, "[%s #%d] Parked at F%d (%d/10 spots)", 
                         vehicle->type, vehicle->id, 10 + vehicle->intersection_id,
                         parking_get_occupancy(intersection->parking_lot));
                display_print_vehicle_log(5, log_msg, get_vehicle_type_color(vehicle->type));
                print_vehicle_status(vehicle->arrival_time, vehicle->type, vehicle->id,
                                    vehicle->origin, "Parking", vehicle->priority, "PARKED");
                graphics_log_event(vehicle->type, "PARKED", vehicle->intersection_id, vehicle->id);
                
                /* SLEEP - SIMULATE PARKING DURATION (with shutdown checks) */
                int park_chunks = 5 + (rand() % 11);  /* 5-15 chunks = 0.5-1.5s */
                for (int p = 0; p < park_chunks; p++) {
                    for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
                        usleep(100000 / 5);
                    if (global_shutdown_flag) {
                        vehicle->is_active = 0;
                        pthread_exit(NULL);
                    }
                }
                
                /* CHECK SHUTDOWN AFTER PARKING */
                if (global_shutdown_flag) {
                    parking_leave_spot(intersection->parking_lot);
                    parking_leave_queue(intersection->parking_lot);
                    vehicle->is_active = 0;
                    pthread_exit(NULL);
                }
                
                /* LEAVE PARKING SPOT AND QUEUE */
                parking_leave_spot(intersection->parking_lot);
                parking_leave_queue(intersection->parking_lot);
                
                fprintf(stderr, "[%s #%d] Left parking spot at F%d\n",
                        vehicle->type, vehicle->id, 10 + vehicle->intersection_id);
            } else {
                /* TIMEOUT WAITING FOR PARKING SPOT */
                parking_leave_queue(intersection->parking_lot);
                fprintf(stderr, "[%s #%d] Parking timeout - proceeding to cross\n",
                        vehicle->type, vehicle->id);
            }
        } else {
            /* QUEUE FULL */
            fprintf(stderr, "[%s #%d] Parking queue full - going to intersection\n",
                    vehicle->type, vehicle->id);
        }
    }

    /* CHECK SHUTDOWN BEFORE CROSSING */
    if (global_shutdown_flag) {
        vehicle->is_active = 0;
        pthread_exit(NULL);
    }

    /* ===== CROSSING SECTION - WAIT UNTIL CAN CROSS ===== */
    vehicle->current_state = CROSSING;
    
    /* BUG #2: Use intersection_can_cross() API instead of inline logic */
    /* This enforces the max-2-concurrent-vehicles constraint properly */
    int wait_iters = 0;
    while (!global_shutdown_flag && wait_iters < 50) {  /* Max 50 iterations * 100ms = 5 seconds */
        if (intersection_can_cross(intersection, is_emergency)) {
            fprintf(stderr, "[%s #%d] Can cross - proceeding\n",
                    vehicle->type, vehicle->id);
            break;  /* Can cross, proceed */
        }
        
        int crossing = intersection_get_crossing_count(intersection);
        fprintf(stderr, "[%s #%d] Waiting - %d vehicles crossing\n",
                vehicle->type, vehicle->id, crossing);
        for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
            usleep(100000 / 5);
        if (global_shutdown_flag) {
            vehicle->is_active = 0;
            pthread_exit(NULL);
        }
        wait_iters++;
    }
    CHECK_SHUTDOWN_AND_EXIT();
    
    /* Check that traffic light is GREEN before crossing */
    vehicle->current_state = WAITING_FOR_SIGNAL;
    
    /* Map origin direction to traffic light */
    traffic_light_t* my_light = NULL;
    if (strcmp(vehicle->origin, "North") == 0) {
        my_light = &intersection->north_light;
    } else if (strcmp(vehicle->origin, "South") == 0) {
        my_light = &intersection->south_light;
    } else if (strcmp(vehicle->origin, "East") == 0) {
        my_light = &intersection->east_light;
    } else if (strcmp(vehicle->origin, "West") == 0) {
        my_light = &intersection->west_light;
    }
    
    /* Wait for light to turn GREEN - max timeout 5 seconds (50 iterations * 100ms) */
    int light_wait_iters = 0;
    while (!global_shutdown_flag && light_wait_iters < 50 && my_light) {
        if (my_light->state == LIGHT_GREEN) {
            fprintf(stderr, "[%s #%d] Light GREEN - proceeding to cross\n",
                    vehicle->type, vehicle->id);
            break;  /* Light is green, proceed to crossing */
        }
        
        fprintf(stderr, "[%s #%d] Waiting - %s light is RED\n",
                vehicle->type, vehicle->id, vehicle->origin);
        for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
            usleep(200000 / 5);  /* Sleep 200ms and retry */
        if (global_shutdown_flag) {
            vehicle->is_active = 0;
            pthread_exit(NULL);
        }
        light_wait_iters++;
    }
    CHECK_SHUTDOWN_AND_EXIT();
    
    /* ADD TO CROSSING COUNT */
    intersection_add_crossing_vehicle(intersection);
    
    /* CHECK SHUTDOWN BEFORE CROSSING */
    if (global_shutdown_flag) {
        intersection_remove_crossing_vehicle(intersection);
        vehicle->is_active = 0;
        pthread_exit(NULL);
    }
    
    /* PRINT DIRECTION MESSAGE */
    const char* dir_str = (vehicle->destination == STRAIGHT) ? "STRAIGHT" :
                         (vehicle->destination == LEFT_TURN) ? "LEFT" : "RIGHT";
    
    memset(log_msg, 0, sizeof(log_msg));
    snprintf(log_msg, sizeof(log_msg) - 1, "[%s #%d] Crossing F%d - %s turn", 
             vehicle->type, vehicle->id, 10 + vehicle->intersection_id, dir_str);
    display_print_vehicle_log(5, log_msg, get_vehicle_type_color(vehicle->type));
    print_vehicle_status(vehicle->arrival_time, vehicle->type, vehicle->id,
                        vehicle->origin, "Intersection", vehicle->priority, "CROSSING");
    graphics_log_event(vehicle->type, "CROSSING", vehicle->intersection_id, vehicle->id);
    
    /* SLEEP FOR CROSSING TIME IN 100MS CHUNKS */
    int cross_chunks = 2 + (rand() % 5);  /* 2-6 chunks = 200-600ms */
    for (int c = 0; c < cross_chunks; c++) {
        for (int _s = 0; _s < 5 && !global_shutdown_flag; _s++)
            usleep(100000 / 5);
        if (global_shutdown_flag) {
            vehicle->is_active = 0;
            pthread_exit(NULL);
        }
    }
    CHECK_SHUTDOWN_AND_EXIT();
    
    /* REMOVE FROM CROSSING COUNT */
    intersection_remove_crossing_vehicle(intersection);

    /* ===== EXIT ===== */
    vehicle->current_state = LEFT_INTERSECTION;
    
    fprintf(stderr, "[%s #%d] Left intersection completely\n",
            vehicle->type, vehicle->id);
    
    print_vehicle_status(vehicle->arrival_time, vehicle->type, vehicle->id,
                        vehicle->origin, "Exit", vehicle->priority, "COMPLETED");
    graphics_log_event(vehicle->type, "COMPLETED", vehicle->intersection_id, vehicle->id);
    
    vehicle->is_active = 0;
    fprintf(stderr, "[%s #%d] Thread exiting (marked inactive)\n",
            vehicle->type, vehicle->id);
    fflush(stderr);
    pthread_exit(NULL);
    return NULL;
}
