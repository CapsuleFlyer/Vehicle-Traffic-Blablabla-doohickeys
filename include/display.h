#ifndef DISPLAY_H
#define DISPLAY_H

#include <time.h>
#include "simulation.h"

/* ===== ANSI COLOR CODES ===== */
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"

/* Legacy aliases for backward compatibility */
#define COLOR_RED     RED
#define COLOR_GREEN   GREEN
#define COLOR_YELLOW  YELLOW
#define COLOR_BLUE    BLUE
#define COLOR_CYAN    CYAN
#define COLOR_WHITE   WHITE
#define COLOR_RESET   RESET

/* ===== EVENT LOGGING FUNCTIONS ===== */
void display_init(void);
void display_print_vehicle_log(int row, const char* message, const char* color);
void display_shutdown(void);

/* ===== VISUAL DISPLAY FUNCTIONS ===== */
void print_banner(void);
void print_intersection_map(void);
const char* get_vehicle_color(const char* vehicle_type);
void print_vehicle_status(time_t arrival_time, const char* vehicle_type, int vehicle_id,
                         const char* origin, const char* destination, 
                         int priority, const char* status);
void print_emergency_alert(const char* vehicle_type, const char* from, const char* to);
void print_live_dashboard(void);
void print_shutdown_summary_detailed(int total_vehicles, int total_parked, 
                                     int emergency_count, int bus_count, 
                                     int car_count, int bike_count, int tractor_count,
                                     int threads_joined, int semaphores_destroyed, int pipes_closed);
void print_final_board(int f10_ambulance, int f10_firetruck, int f10_bus, int f10_car, int f10_bike, int f10_tractor,
                       int f11_ambulance, int f11_firetruck, int f11_bus, int f11_car, int f11_bike, int f11_tractor,
                       int f10_spots, int f10_queue, int f11_spots, int f11_queue);
void print_shutdown(void);

#endif 
