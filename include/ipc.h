#ifndef IPC_H
#define IPC_H

#include "vehicle.h"
#include <errno.h>
#include <fcntl.h>

/* IPC Message types */
typedef enum {
    MSG_EMERGENCY_ALERT,
    MSG_EMERGENCY_CLEAR,
    MSG_TRAFFIC_STATUS,
    MSG_COORDINATION_REQUEST,
    MSG_COORDINATION_ACK
} msg_type_t;

/* IPC Message structure */
typedef struct {
    msg_type_t type;
    int source_intersection;  /* 0 for F10, 1 for F11 */
    int vehicle_id;
    char vehicle_type[20];
    int priority;
    int data;  /* auxiliary data */
} ipc_message_t;

/* Pipe pair structure */
typedef struct {
    int pipe_f10_to_f11[2];  /* F10 writes, F11 reads */
    int pipe_f11_to_f10[2];  /* F11 writes, F10 reads */
} pipe_pair_t;

/* Function prototypes */
pipe_pair_t* ipc_create_pipes(void);
void ipc_destroy_pipes(pipe_pair_t* pipes);
int ipc_send_message(pipe_pair_t* pipes, int from_intersection, ipc_message_t* msg);
int ipc_receive_message(pipe_pair_t* pipes, int to_intersection, ipc_message_t* msg);
int ipc_send_emergency_alert(pipe_pair_t* pipes, int source, vehicle_t* vehicle);

#endif /* IPC_H */
