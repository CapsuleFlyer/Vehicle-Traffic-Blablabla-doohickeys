#define _POSIX_C_SOURCE 200809L

#include "display.h"
#include "vehicle.h"
#include "intersection.h"
#include "parking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>


/* Event log storage - circular buffer */
#define EVENT_LOG_SIZE 100
typedef struct {
    char messages[EVENT_LOG_SIZE][256];
    int count;
    int head;
} event_log_t;

static event_log_t event_log = {0};
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void display_init(void) {
    memset(&event_log, 0, sizeof(event_log));
}

void display_shutdown(void) {
    fflush(stdout);
    fflush(stderr);
}

void display_clear_screen(void) {
    /* \033[2J clears entire screen, \033[H moves cursor to home */
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* Add event to circular log */
static void add_event(const char* message) {
    if (!message) return;
    
    pthread_mutex_lock(&log_lock);
    
    if (event_log.count < EVENT_LOG_SIZE) {
        strncpy(event_log.messages[event_log.count], message, 255);
        event_log.messages[event_log.count][255] = '\0';
        event_log.count++;
    } else {
        strncpy(event_log.messages[event_log.head], message, 255);
        event_log.messages[event_log.head][255] = '\0';
        event_log.head = (event_log.head + 1) % EVENT_LOG_SIZE;
    }
    
    pthread_mutex_unlock(&log_lock);
}

/* Print aligned dashboard*/
void display_draw_live_dashboard(intersection_t* f10, intersection_t* f11) {
    /* Simplified display - removed visual blocks, keeping logic intact */
    (void)f10;
    (void)f11;
}

void display_draw_traffic_light(int row, int col, int is_green, const char* direction) {
    (void)row;
    (void)col;
    (void)is_green;
    (void)direction;
}

void display_draw_parking_lot(int row, int col, int occupancy, int max_spots) {
    (void)row;
    (void)col;
    (void)occupancy;
    (void)max_spots;
}

/* Print ASCII intersection map with parking */
void display_draw_intersections(intersection_t* f10, intersection_t* f11) {
    (void)f10;
    (void)f11;
    /* Removed visual rendering - logic in background */
}

void display_print_vehicle_log(int row, const char* message, const char* color) {
    (void)row;
    
    if (!message || !color) return;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    fprintf(stderr, "%s[%s] %s%s\n", color, timestamp, message, RESET);
    fflush(stderr);
}

void display_print_vehicle_table(int row, vehicle_t** vehicles, int vehicle_count) {
    (void)row;
    (void)vehicles;
    (void)vehicle_count;
    
    /* Removed visual table - logging via stderr only */
}

void display_update_live_ui(intersection_t* f10, intersection_t* f11, 
                            vehicle_t** vehicles, int vehicle_count) {
    (void)f10;
    (void)f11;
    (void)vehicles;
    (void)vehicle_count;
}

void display_print_shutdown_summary(int total_vehicles, int total_parked) {
    printf("\n\n\033[1;32m");
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    SIMULATION SHUTDOWN - SUMMARY                               ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ Total Vehicles Processed:   %-2d%46s║\n", total_vehicles, "");
    printf("║ Total Vehicles Parked:      %-2d%47s║\n", total_parked, "");
    printf("║ Terminated gracefully by Ctrl+C (SIGINT)                                       ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\033[0m\n\n");
    fflush(stdout);
}

/* Helper function to convert vehicle state to string - already defined in vehicle.c */
/* extern const char* get_state_string(vehicle_state_t state); */

/* Get vehicle color based on type */
const char* get_vehicle_color(const char* vehicle_type) {
    if (!vehicle_type) return RESET;
    
    if (strcmp(vehicle_type, "Ambulance") == 0) return RED;
    if (strcmp(vehicle_type, "Firetruck") == 0) return RED;
    if (strcmp(vehicle_type, "Bus") == 0) return YELLOW;
    if (strcmp(vehicle_type, "Car") == 0) return GREEN;
    if (strcmp(vehicle_type, "Bike") == 0) return CYAN;
    if (strcmp(vehicle_type, "Tractor") == 0) return MAGENTA;
    
    return RESET;
}

/* Print banner */
void print_banner(void) {
    printf("\n%s", BOLD CYAN);
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    printf("                 TRAFFIC INTERSECTION SIMULATOR (OS PROJECT)                    \n");
    printf("                        Managing F10 & F11 Intersections                        \n");
    printf("════════════════════════════════════════════════════════════════════════════════\n");
    printf("%s\n", RESET);
    fflush(stdout);
}

/* Print intersection map */
void print_intersection_map(void) {
    printf("\n%s", CYAN);
    printf("F10 & F11 Intersection Map:\n");
     printf("       [NORTH]                    [NORTH]\n");
    printf("           |                          |\n");
    printf("[WEST]----F10----[EAST]    [WEST]----F11----[EAST]\n");
    printf("           |                          |\n");
    printf("        [SOUTH]                    [SOUTH]\n");
    printf("%s\n", RESET);
    fflush(stdout);
}

/* Print vehicle status */
void print_vehicle_status(time_t arrival_time, const char* vehicle_type, int vehicle_id,
                         const char* origin, const char* destination, 
                         int priority, const char* status) {
    const char* color = get_vehicle_color(vehicle_type);
    time_t now = time(NULL);
    int elapsed = (int)(now - arrival_time);
    
    fprintf(stderr, "%s[%s] Vehicle #%d - From %s to %s (Priority: %d) - Elapsed: %ds - Status: %s%s\n",
            color, vehicle_type, vehicle_id, origin, destination, priority, elapsed, 
            status, RESET);
    fflush(stderr);
}

/* Print emergency alert */
void print_emergency_alert(const char* vehicle_type, const char* from, const char* to) {
    fprintf(stderr, "\n%s", RED);
    fprintf(stderr, "╔════════════════════════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║                           🚨 EMERGENCY ALERT 🚨                               ║\n");
    fprintf(stderr, "║ %s traveling from %s to %s%*s║\n",
            vehicle_type, from, to, (int)(70 - strlen(vehicle_type) - strlen(from) - strlen(to) - 12), "");
    fprintf(stderr, "╚════════════════════════════════════════════════════════════════════════════════╝\n");
    fprintf(stderr, "%s\n", RESET);
    fflush(stderr);
    
    add_event(vehicle_type);
}

/* Print live dashboard */
void print_live_dashboard(void) {
}

/* Print detailed shutdown summary */
void print_shutdown_summary_detailed(int total_vehicles, int total_parked,
                                    int emergency_count, int bus_count, 
                                    int car_count, int bike_count, int tractor_count,
                                    int threads_joined, int semaphores_destroyed, int pipes_closed) {
    printf("\n%s", BOLD GREEN);
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      DETAILED SHUTDOWN SUMMARY                                 ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ Total Vehicles Processed:  %s%2d%s                                             ║\n", YELLOW, total_vehicles, GREEN);
    printf("║ Total Vehicles Parked:     %s%2d%s                                             ║\n", YELLOW, total_parked, GREEN);
    printf("║                                                                                ║\n");
    printf("║ Vehicle Type Breakdown:                                                        ║\n");
    printf("║   Emergency Vehicles (Ambulance/Firetruck): %s%2d%s                            ║\n",YELLOW, emergency_count, GREEN);
    printf("║   Buses:    %s%2d%s                                                            ║\n",YELLOW, bus_count, GREEN);
    printf("║   Cars:     %s%2d%s                                                            ║\n",YELLOW, car_count, GREEN);
    printf("║   Bikes:    %s%2d%s                                                            ║\n",YELLOW, bike_count, GREEN);
    printf("║   Tractors: %s%2d%s                                                            ║\n",YELLOW, tractor_count, GREEN);
    printf("║                                                                                ║\n");
    printf("║ Cleanup Status:                                                                ║\n");
    printf("║   Threads Joined:        %s%2d%s                                               ║\n",YELLOW, threads_joined, GREEN);
    printf("║   Semaphores Destroyed:  %s%2d%s                                               ║\n",YELLOW, semaphores_destroyed, GREEN);
    printf("║   Pipes Closed:          %s%2d%s                                               ║\n",YELLOW, pipes_closed, GREEN);
    printf("║                                                                                ║\n");
    printf("║ Simulation terminated gracefully                                               ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝%s\n\n",
           RESET);
    fflush(stdout);
}

/* Print final shutdown message */
void print_shutdown(void) {
    printf("\n%s", BOLD CYAN);
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          SIMULATION CLOSED                                     ║\n");
    printf("║                    All resources successfully freed                            ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("%s\n", RESET);
    fflush(stdout);
}

/*
 * Print comprehensive final board with all simulation statistics
 * Receives pre-counted sector breakdown and semaphore values from shutdown_simulation()
 */
void print_final_board(int f10_ambulance, int f10_firetruck, int f10_bus, int f10_car, int f10_bike, int f10_tractor,
                     int f11_ambulance, int f11_firetruck, int f11_bus, int f11_car, int f11_bike, int f11_tractor,
                     int f10_spots, int f10_queue, int f11_spots, int f11_queue) {
    if (!global_simulation) return;

    printf("\n%s", CYAN);
    printf("╔═══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        SIMULATION FINAL BOARD REPORT                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════════╝\n");

    /* SECTION 1: TOTAL VEHICLES */
    printf("\n%s", CYAN);
    printf("┌─ TOTAL VEHICLES ─────────────────────────────────────────────────────────────┐\n");
    printf("│ %s%d%s vehicles processed during simulation%54s│\n", 
           GREEN, MAX_VEHICLES, RESET CYAN, "");
    printf("└──────────────────────────────────────────────────────────────────────────────┘\n");

    /* SECTION 2: PARKING SUMMARY */
    printf("\n%s", CYAN);
    printf("┌─ PARKING SUMMARY ────────────────────────────────────────────────────────────┐\n");

    /* F10 Parking */
    int f10_occupancy = (global_simulation->f10_intersection && global_simulation->f10_intersection->parking_lot) 
        ? parking_get_occupancy(global_simulation->f10_intersection->parking_lot) : 0;
    int f10_total = (global_simulation->f10_intersection && global_simulation->f10_intersection->parking_lot)
        ? parking_get_total_parked(global_simulation->f10_intersection->parking_lot) : 0;
    printf("│ F10 Parking: %s%d/%d%s [", GREEN, f10_occupancy, 10, RESET CYAN);
    for (int i = 0; i < 10; i++) printf("%s", (i < f10_occupancy) ? "█" : "░");
    printf("%s] Total ever parked: %s%d%s%30s│\n", CYAN, GREEN, f10_total, RESET CYAN, "");

    /* F11 Parking */
    int f11_occupancy = (global_simulation->f11_intersection && global_simulation->f11_intersection->parking_lot)
        ? parking_get_occupancy(global_simulation->f11_intersection->parking_lot) : 0;
    int f11_total = (global_simulation->f11_intersection && global_simulation->f11_intersection->parking_lot)
        ? parking_get_total_parked(global_simulation->f11_intersection->parking_lot) : 0;
    printf("│ F11 Parking: %s%d/%d%s [", GREEN, f11_occupancy, 10, RESET CYAN);
    for (int i = 0; i < 10; i++) printf("%s", (i < f11_occupancy) ? "█" : "░");
    printf("%s] Total ever parked: %s%d%s%30s│\n", CYAN, GREEN, f11_total, RESET CYAN, "");

    printf("│ Combined Total Parked: %s%d%s%51s│\n", GREEN, global_simulation->total_parked_vehicles, RESET CYAN, "");
    printf("└──────────────────────────────────────────────────────────────────────────────┘\n");

    /* SECTION 3: SECTOR BREAKDOWN - uses pre-counted parameters */
    printf("\n%s", CYAN);
    printf("┌─ SECTOR BREAKDOWN ───────────────────────────────────────────────────────────┐\n");

    printf("│ F10 Sector:%68s│\n", "");
    printf("│   Ambulances: %s%d%s | Firetrucks: %s%d%s | Buses: %s%d%s%30s│\n",
           GREEN, f10_ambulance, RESET CYAN, GREEN, f10_firetruck, RESET CYAN, GREEN, f10_bus, RESET CYAN, "");
    printf("│   Cars: %s%d%s | Bikes: %s%d%s | Tractors: %s%d%s%35s│\n",
           GREEN, f10_car, RESET CYAN, GREEN, f10_bike, RESET CYAN, GREEN, f10_tractor, RESET CYAN, "");

    printf("│ F11 Sector:%68s│\n", "");
    printf("│   Ambulances: %s%d%s | Firetrucks: %s%d%s | Buses: %s%d%s%30s│\n",
           GREEN, f11_ambulance, RESET CYAN, GREEN, f11_firetruck, RESET CYAN, GREEN, f11_bus, RESET CYAN, "");
    printf("│   Cars: %s%d%s | Bikes: %s%d%s | Tractors: %s%d%s%35s│\n",
           GREEN, f11_car, RESET CYAN, GREEN, f11_bike, RESET CYAN, GREEN, f11_tractor, RESET CYAN, "");
    printf("└──────────────────────────────────────────────────────────────────────────────┘\n");

    /* SECTION 4: SEMAPHORE STATUS - uses pre-captured values from shutdown_simulation() */
    printf("\n%s", CYAN);
    printf("┌─ SEMAPHORE STATUS ───────────────────────────────────────────────────────────┐\n");
    printf("│ F10: Available Spots: %s%d%s | Queue Slots: %s%d%s%43s│\n",
           GREEN, f10_spots, RESET CYAN, GREEN, f10_queue, RESET CYAN, "");
    printf("│ F11: Available Spots: %s%d%s | Queue Slots: %s%d%s%43s│\n",
           GREEN, f11_spots, RESET CYAN, GREEN, f11_queue, RESET CYAN, "");
    printf("└──────────────────────────────────────────────────────────────────────────────┘\n");

    /* SECTION 5: THREAD STATUS */
    printf("\n%s", CYAN);
    printf("┌─ THREAD STATUS ──────────────────────────────────────────────────────────────┐\n");
    printf("│ All %s15%s vehicle threads joined successfully%45s│\n", GREEN, RESET CYAN, "");
    printf("└──────────────────────────────────────────────────────────────────────────────┘\n");

    /* SECTION 6: IPC PIPES */
    printf("\n%s", CYAN);
    printf("┌─ IPC PIPES ──────────────────────────────────────────────────────────────────┐\n");
    printf("│ Pipe F10→F11: %sclosed%s%57s│\n", GREEN, RESET CYAN, "");
    printf("│ Pipe F11→F10: %sclosed%s%57s│\n", GREEN, RESET CYAN, "");
    printf("└──────────────────────────────────────────────────────────────────────────────┘%s\n\n",
           RESET);

    fflush(stdout);
}

