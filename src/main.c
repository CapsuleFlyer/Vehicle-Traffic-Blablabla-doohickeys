#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "simulation.h"
#include "display.h"
#include "vehicle.h"
#include "intersection.h"
#include "parking.h"
#include "ipc.h"
#include "graphics.h"

/* Global simulation state */
simulation_t* global_simulation = NULL;
volatile int global_shutdown_flag = 0;

/*
 * Signal handler for SIGINT (Ctrl+C)
 */
void signal_handler(int sig) {
    if (sig == SIGINT) {
        global_shutdown_flag = 1;
        if (global_simulation) {
            global_simulation->shutdown_flag = 1;
            global_simulation->spawn_active = 0;
        }
    }
}

/*
 * Setup signal handlers
 */
void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;  /* CRITICAL: Restart interrupted system calls */
    sigaction(SIGINT, &sa, NULL);
}

/*
 * Controller process for F10
 */
void f10_controller_process(intersection_t* intersection, pipe_pair_t* pipes) {
    printf("[F10 CONTROLLER] Process started (PID: %d)\n", getpid());
    fflush(stdout);
    
    while (!global_shutdown_flag) {
        /* Check for emergency messages from F11 */
        ipc_message_t msg;
        memset(&msg, 0, sizeof(ipc_message_t));
        if (ipc_receive_message(pipes, 0, &msg) == 0) {
            if (msg.type == MSG_EMERGENCY_ALERT) {
                fprintf(stderr, "[F10] Received EMERGENCY alert from F11 for %s #%d\n", 
                       msg.vehicle_type, msg.vehicle_id);
                fflush(stderr);
                // VISUAL ADDED
                print_emergency_alert(msg.vehicle_type, "F11", "F10");
                intersection_set_emergency_mode(intersection, 1);
                usleep(500000);
                intersection_set_emergency_mode(intersection, 0);
            }
        }
        
        usleep(100000);  /* Check every 100ms */
    }
    
    printf("[F10 CONTROLLER] Shutting down gracefully\n");
    fflush(stdout);
    exit(0);
}

/*
 * Controller process for F11
 */
void f11_controller_process(intersection_t* intersection, pipe_pair_t* pipes) {
    printf("[F11 CONTROLLER] Process started (PID: %d)\n", getpid());
    fflush(stdout);
    
    while (!global_shutdown_flag) {
        /* Check for emergency messages from F10 */
        ipc_message_t msg;
        memset(&msg, 0, sizeof(ipc_message_t));
        if (ipc_receive_message(pipes, 1, &msg) == 0) {
            if (msg.type == MSG_EMERGENCY_ALERT) {
                fprintf(stderr, "[F11] Received EMERGENCY alert from F10 for %s #%d\n", 
                       msg.vehicle_type, msg.vehicle_id);
                fflush(stderr);
                // VISUAL ADDED
                print_emergency_alert(msg.vehicle_type, "F10", "F11");
                intersection_set_emergency_mode(intersection, 1);
                usleep(500000);
                intersection_set_emergency_mode(intersection, 0);
            }
        }
        
        usleep(100000);  /* Check every 100ms */
    }
    
    printf("[F11 CONTROLLER] Shutting down gracefully\n");
    fflush(stdout);
    exit(0);
}

/*
 * Spawn vehicle threads
 */
void spawn_vehicle_threads(simulation_t* sim) {
    /* Define pool of all 6 vehicle types with their priorities */
    const char* pool_types[] = {"Ambulance", "Firetruck", "Bus", "Car", "Bike", "Tractor"};
    int pool_priorities[] = {PRIORITY_EMERGENCY, PRIORITY_EMERGENCY, PRIORITY_MEDIUM, 
                             PRIORITY_NORMAL, PRIORITY_NORMAL, PRIORITY_NORMAL};
    
    /* Build vehicle list: 6 guaranteed (one of each type) + random fills for remaining slots */
    char* vehicle_types[MAX_VEHICLES];
    int priority_map[MAX_VEHICLES];
    
    /* 1. Add one guaranteed vehicle of each type */
    for (int i = 0; i < 6; i++) {
        vehicle_types[i] = (char*)pool_types[i];
        priority_map[i] = pool_priorities[i];
    }
    
    /* 2. Randomly fill remaining slots using rand() % 6 */
    int remaining = MAX_VEHICLES - 6;
    for (int i = 0; i < remaining; i++) {
        int pool_index = rand() % 6;
        vehicle_types[6 + i] = (char*)pool_types[pool_index];
        priority_map[6 + i] = pool_priorities[pool_index];
    }
    
    /* VISUAL ADDED: Randomize spawn order */
    int indices[MAX_VEHICLES];
    for (int i = 0; i < MAX_VEHICLES; i++) {
        indices[i] = i;
    }
    /* Fisher-Yates shuffle */
    for (int i = MAX_VEHICLES - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    for (int spawn_order = 0; spawn_order < MAX_VEHICLES && !global_shutdown_flag; spawn_order++) {
        int i = indices[spawn_order];
        
        /* FIXED: Use spawn_order for F10/F11 balance, not shuffled index */
        int intersection_id = spawn_order % 2;
        
        /* CRITICAL: Bounds check to prevent array overrun */
        if (i >= (int)(sizeof(vehicle_types) / sizeof(vehicle_types[0])) ||
            i >= (int)(sizeof(priority_map) / sizeof(priority_map[0]))) {
            fprintf(stderr, "ERROR: Vehicle index %d out of bounds!\n", i);
            break;
        }
        
        /* Create vehicle */
        vehicle_t* vehicle = create_vehicle(i + 1, vehicle_types[i], 
                                           priority_map[i], intersection_id);
        if (!vehicle) {
            fprintf(stderr, "Failed to create vehicle %d\n", i + 1);
            continue;
        }

        sim->vehicles[i] = vehicle;
        
        fprintf(stderr, "[SPAWN] Creating vehicle #%d (%s) at F%d\n", 
               vehicle->id, vehicle->type, 10 + intersection_id);
        fflush(stderr);
        
        /* Create thread for this vehicle */
        if (pthread_create(&vehicle->thread_id, NULL, vehicle_thread_func, vehicle) != 0) {
            fprintf(stderr, "Failed to create thread for vehicle %d\n", vehicle->id);
            destroy_vehicle(vehicle);
            sim->vehicles[i] = NULL;
            continue;
        }

        /* Update SFML status */
        char smsg[128];
        snprintf(smsg, sizeof(smsg), "Spawned %s #%d at F%d — waiting for next vehicle...",
                 vehicle->type, vehicle->id, 10 + intersection_id);
        graphics_set_status(smsg, spawn_order + 1);

        /* Random spawn interval - one by one */
        usleep(200000 + (rand() % 300000));
    }
    
    fprintf(stderr, "[SPAWN] All %d vehicle threads created successfully.\n", MAX_VEHICLES);
    fflush(stderr);
}

/*
 * Shutdown simulation gracefully
 */
void shutdown_simulation(void) {
    if (!global_simulation) return;
    
    /* COUNT VEHICLE TYPES FIRST - before destroying vehicles */
    int emergency_count = 0, bus_count = 0, car_count = 0, bike_count = 0, tractor_count = 0;
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (global_simulation->vehicles[i]) {
            const char* t = global_simulation->vehicles[i]->type;
            if (strcmp(t, "Ambulance") == 0 || strcmp(t, "Firetruck") == 0)
                emergency_count++;
            else if (strcmp(t, "Bus") == 0) bus_count++;
            else if (strcmp(t, "Car") == 0) car_count++;
            else if (strcmp(t, "Bike") == 0) bike_count++;
            else if (strcmp(t, "Tractor") == 0) tractor_count++;
        }
    }
    
    /* BUG FIX #1: COUNT VEHICLE TYPES PER SECTOR - before destroying vehicles */
    int f10_ambulance = 0, f10_firetruck = 0, f10_bus = 0, f10_car = 0, f10_bike = 0, f10_tractor = 0;
    int f11_ambulance = 0, f11_firetruck = 0, f11_bus = 0, f11_car = 0, f11_bike = 0, f11_tractor = 0;
    
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (global_simulation->vehicles[i]) {
            const char* type = global_simulation->vehicles[i]->type;
            int sector = global_simulation->vehicles[i]->intersection_id;
            
            if (sector == 0) {  /* F10 */
                if (strcmp(type, "Ambulance") == 0) f10_ambulance++;
                else if (strcmp(type, "Firetruck") == 0) f10_firetruck++;
                else if (strcmp(type, "Bus") == 0) f10_bus++;
                else if (strcmp(type, "Car") == 0) f10_car++;
                else if (strcmp(type, "Bike") == 0) f10_bike++;
                else if (strcmp(type, "Tractor") == 0) f10_tractor++;
            } else if (sector == 1) {  /* F11 */
                if (strcmp(type, "Ambulance") == 0) f11_ambulance++;
                else if (strcmp(type, "Firetruck") == 0) f11_firetruck++;
                else if (strcmp(type, "Bus") == 0) f11_bus++;
                else if (strcmp(type, "Car") == 0) f11_car++;
                else if (strcmp(type, "Bike") == 0) f11_bike++;
                else if (strcmp(type, "Tractor") == 0) f11_tractor++;
            }
        }
    }

    fprintf(stderr, "\n[MAIN] Shutdown signal received. Cleaning up...\n");
    fflush(stderr);
    global_simulation->spawn_active = 0;

    /* Wait for all vehicle threads */
    fprintf(stderr, "[MAIN] Waiting for %d vehicle threads to finish...\n", MAX_VEHICLES);
    fflush(stderr);
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (global_simulation->vehicles[i]) {
            fprintf(stderr, "[MAIN]   Joining vehicle #%d...\n", i + 1);
            fflush(stderr);
            pthread_join(global_simulation->vehicles[i]->thread_id, NULL);
            destroy_vehicle(global_simulation->vehicles[i]);
        }
    }

    /* Terminate controller processes */
    fprintf(stderr, "[MAIN] Terminating controller processes...\n");
    fflush(stderr);
    if (global_simulation->f10_controller_pid > 0) {
        kill(global_simulation->f10_controller_pid, SIGTERM);
        waitpid(global_simulation->f10_controller_pid, NULL, 0);
    }
    if (global_simulation->f11_controller_pid > 0) {
        kill(global_simulation->f11_controller_pid, SIGTERM);
        waitpid(global_simulation->f11_controller_pid, NULL, 0);
    }

    /* BUG FIX #2: GET SEMAPHORE VALUES BEFORE DESTROYING INTERSECTIONS */
    int f10_spots = 0, f10_queue = 0, f11_spots = 0, f11_queue = 0;
    if (global_simulation->f10_intersection && global_simulation->f10_intersection->parking_lot) {
        sem_getvalue(&global_simulation->f10_intersection->parking_lot->parking_spots, &f10_spots);
        sem_getvalue(&global_simulation->f10_intersection->parking_lot->waiting_queue, &f10_queue);
    }
    if (global_simulation->f11_intersection && global_simulation->f11_intersection->parking_lot) {
        sem_getvalue(&global_simulation->f11_intersection->parking_lot->parking_spots, &f11_spots);
        sem_getvalue(&global_simulation->f11_intersection->parking_lot->waiting_queue, &f11_queue);
    }

    /* Destroy intersections and pipes */
    fprintf(stderr, "[MAIN] Cleaning up resources...\n");
    fflush(stderr);
    if (global_simulation->f10_intersection) {
        intersection_destroy(global_simulation->f10_intersection);
    }
    if (global_simulation->f11_intersection) {
        intersection_destroy(global_simulation->f11_intersection);
    }
    if (global_simulation->ipc_pipes) {
        ipc_destroy_pipes(global_simulation->ipc_pipes);
    }

    /* Display shutdown summary with pre-counted values */
    print_shutdown_summary_detailed(MAX_VEHICLES, global_simulation->total_parked_vehicles,
                                    emergency_count, bus_count, car_count, bike_count, tractor_count,
                                    MAX_VEHICLES, 4, 1);
    
    /* Print final board summary with pre-counted sector and semaphore values */
    print_final_board(f10_ambulance, f10_firetruck, f10_bus, f10_car, f10_bike, f10_tractor,
                     f11_ambulance, f11_firetruck, f11_bus, f11_car, f11_bike, f11_tractor,
                     f10_spots, f10_queue, f11_spots, f11_queue);

    /* Show final summary in SFML window */
    graphics_set_status("Simulation complete!", 15);
    graphics_show_final(MAX_VEHICLES, global_simulation->total_parked_vehicles,
                        emergency_count, bus_count, car_count, bike_count, tractor_count,
                        global_simulation->f10_intersection ?
                            parking_get_occupancy(global_simulation->f10_intersection->parking_lot) : 0,
                        global_simulation->f11_intersection ?
                            parking_get_occupancy(global_simulation->f11_intersection->parking_lot) : 0);

    /* Shutdown SFML graphics window */
    graphics_shutdown();

    /* Free simulation structure */
    fprintf(stderr, "[MAIN] Freeing simulation memory...\n");
    fflush(stderr);
    
    /* BUG #3: Destroy the parked_count_lock mutex */
    pthread_mutex_destroy(&global_simulation->parked_count_lock);
    
    free(global_simulation->vehicles);
    free(global_simulation);
    global_simulation = NULL;
    
    fprintf(stderr, "[MAIN] Cleanup complete!\n");
    fflush(stderr);
    
    print_shutdown();
}

/*
 * Main entry point
 */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Traffic Intersection Simulator (F10 & F11)\n");
    printf("  OS Concepts: Threads, Processes, IPC, Synchronization\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    srand(time(NULL));
    setup_signal_handlers();

    /* Create simulation structure */
    global_simulation = (simulation_t*)malloc(sizeof(simulation_t));
    if (!global_simulation) {
        perror("malloc");
        return 1;
    }

    /* Initialize simulation */
    global_simulation->vehicle_count = MAX_VEHICLES;
    global_simulation->shutdown_flag = 0;
    global_simulation->spawn_active = 1;
    global_simulation->total_parked_vehicles = 0;
    
    /* BUG #3: Initialize mutex for protecting total_parked_vehicles */
    pthread_mutex_init(&global_simulation->parked_count_lock, NULL);
    
    global_simulation->vehicles = (vehicle_t**)malloc(MAX_VEHICLES * sizeof(vehicle_t*));
    if (!global_simulation->vehicles) {
        perror("malloc");
        free(global_simulation);
        return 1;
    }
    memset(global_simulation->vehicles, 0, MAX_VEHICLES * sizeof(vehicle_t*));

    /* Create intersections */
    printf("[MAIN] Creating intersections F10 and F11...\n");
    global_simulation->f10_intersection = intersection_create(0, 12000, 5000);
    global_simulation->f11_intersection = intersection_create(1, 12000, 5000);
    
    if (!global_simulation->f10_intersection || !global_simulation->f11_intersection) {
        printf("Failed to create intersections\n");
        shutdown_simulation();
        return 1;
    }

    /* Create IPC pipes */
    printf("[MAIN] Creating IPC pipes between F10 and F11...\n");
    global_simulation->ipc_pipes = ipc_create_pipes();
    if (!global_simulation->ipc_pipes) {
        printf("Failed to create pipes\n");
        shutdown_simulation();
        return 1;
    }

    /* Fork F10 controller */
    printf("[MAIN] Forking F10 controller process...\n");
    global_simulation->f10_controller_pid = fork();
    if (global_simulation->f10_controller_pid == 0) {
        /* Child process: F10 controller */
        /* ISSUE #2: Close unused pipe ends in child process */
        /* F10 reads from pipe_f11_to_f10[0], writes to pipe_f10_to_f11[1] */
        close(global_simulation->ipc_pipes->pipe_f10_to_f11[0]);  /* Don't read own pipe */
        close(global_simulation->ipc_pipes->pipe_f11_to_f10[1]);  /* Don't write to other's pipe */
        
        f10_controller_process(global_simulation->f10_intersection, 
                               global_simulation->ipc_pipes);
        exit(0);
    } else if (global_simulation->f10_controller_pid < 0) {
        perror("fork");
        shutdown_simulation();
        return 1;
    }

    /* Fork F11 controller */
    printf("[MAIN] Forking F11 controller process...\n");
    global_simulation->f11_controller_pid = fork();
    if (global_simulation->f11_controller_pid == 0) {
        /* Child process: F11 controller */
        /* ISSUE #2: Close unused pipe ends in child process */
        /* F11 reads from pipe_f10_to_f11[0], writes to pipe_f11_to_f10[1] */
        close(global_simulation->ipc_pipes->pipe_f11_to_f10[0]);  /* Don't read own pipe */
        close(global_simulation->ipc_pipes->pipe_f10_to_f11[1]);  /* Don't write to other's pipe */
        
        f11_controller_process(global_simulation->f11_intersection, 
                               global_simulation->ipc_pipes);
        exit(0);
    } else if (global_simulation->f11_controller_pid < 0) {
        perror("fork");
        shutdown_simulation();
        return 1;
    }
    
    /* NOTE: Parent keeps pipe ends open so vehicle threads can send emergency alerts */
    /* Kernel will clean up pipes when all processes exit */

    /* Print startup messages */
    print_banner();
    print_intersection_map();
    graphics_init();

    printf("[MAIN] Starting vehicle spawn threads...\n");
    usleep(500000);

    /* Spawn vehicle threads */
    spawn_vehicle_threads(global_simulation);

    /* Main loop: update UI and wait for shutdown signal or all vehicles complete */
    printf("[MAIN] Simulation running. Press Ctrl+C to stop.\n\n");
    fflush(stdout);
    
    int idle_count = 0;  /* Count cycles with no active vehicles */
    int last_active = MAX_VEHICLES;
    int wait_cycles = 0;  /* Wait at least 20 cycles (10 seconds) before checking for completion */
    int dashboard_cycles = 0;  /* Display dashboard periodically */
    
    while (!global_shutdown_flag) {
        dashboard_cycles++;

        /* Update traffic lights in parent process - controllers can't do this
           correctly because fork() gives them a separate copy of the memory */
        intersection_update_lights(global_simulation->f10_intersection);
        intersection_update_lights(global_simulation->f11_intersection);

        /* Check if all vehicles are finished */
        int active_vehicles = 0;
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (global_simulation->vehicles[i] && global_simulation->vehicles[i]->is_active) {
                active_vehicles++;
            }
        }
        
        /* Print status when active vehicle count changes */
        if (active_vehicles != last_active) {
            fprintf(stderr, "[MAIN] Active vehicles: %d/%d\n", active_vehicles, MAX_VEHICLES);
            fflush(stderr);
            last_active = active_vehicles;
        }

        /* Update SFML graphics window every 2 cycles */
        if (dashboard_cycles % 2 == 0) {
            if (global_simulation->f10_intersection && global_simulation->f11_intersection) {
                graphics_update_state(
                    global_simulation->f10_intersection->vehicles_crossing,
                    parking_get_occupancy(global_simulation->f10_intersection->parking_lot),
                    global_simulation->f11_intersection->vehicles_crossing,
                    parking_get_occupancy(global_simulation->f11_intersection->parking_lot),
                    global_simulation->f10_intersection->north_light.state,
                    global_simulation->f10_intersection->east_light.state,
                    global_simulation->f11_intersection->north_light.state,
                    global_simulation->f11_intersection->east_light.state,
                    global_simulation->f10_intersection->emergency_vehicle_present,
                    global_simulation->f11_intersection->emergency_vehicle_present,
                    global_simulation->total_parked_vehicles,
                    active_vehicles
                );
            }
            usleep(100000);
        }
        
        /* Update SFML status bar */
        if (active_vehicles > 0) {
            graphics_set_status("Simulation running — vehicles processing...", 15);
        } else {
            graphics_set_status("All vehicles done — waiting to confirm shutdown...", 15);
        }

        wait_cycles++;  /* Always increment so completion check works */
        
        /* If no active vehicles for 10 cycles (5 seconds) AND at least 30 cycles (15 seconds) have passed, auto-shutdown */
        if (active_vehicles == 0 && wait_cycles >= 30) {
            idle_count++;
            if (idle_count >= 10) {
                fprintf(stderr, "\n[MAIN] All vehicles processed. Auto-shutting down...\n");
                fflush(stderr);
                global_shutdown_flag = 1;
                break;
            }
        } else {
            idle_count = 0;  /* Reset idle counter if vehicles are still active */
        }
        
        usleep(500000);  /* Check every 500ms */
    }

    /* Graceful shutdown */
    printf("\n");
    fflush(stdout);
    shutdown_simulation();

    printf("\n[MAIN] Simulation terminated successfully.\n");
    fflush(stdout);
    return 0;
}
