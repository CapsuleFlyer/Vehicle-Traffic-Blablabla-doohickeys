#ifndef GRAPHICS_H
#define GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

void graphics_init(void);

void graphics_log_event(const char* vehicle_type, const char* status,
                        int intersection_id, int vehicle_id);

void graphics_update_state(
    int f10_crossing, int f10_occupancy,
    int f11_crossing, int f11_occupancy,
    int f10_north_green, int f10_east_green,
    int f11_north_green, int f11_east_green,
    int f10_emergency, int f11_emergency,
    int total_parked, int active_vehicles
);

void graphics_set_status(const char* msg, int spawned_count);

void graphics_show_final(int total_vehicles, int total_parked,
                         int emergency_count, int bus_count,
                         int car_count, int bike_count, int tractor_count,
                         int f10_occupancy, int f11_occupancy);

void graphics_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
