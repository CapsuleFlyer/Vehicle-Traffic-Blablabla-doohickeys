#define _XOPEN_SOURCE 700
#include "ipc.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/*
 * Create bidirectional pipes for IPC between F10 and F11
 */
pipe_pair_t* ipc_create_pipes(void) {
    pipe_pair_t* pipes = (pipe_pair_t*)malloc(sizeof(pipe_pair_t));
    if (!pipes) {
        perror("malloc");
        return NULL;
    }

    /* Create F10 -> F11 pipe */
    if (pipe(pipes->pipe_f10_to_f11) == -1) {
        perror("pipe");
        free(pipes);
        return NULL;
    }

    /* Create F11 -> F10 pipe */
    if (pipe(pipes->pipe_f11_to_f10) == -1) {
        perror("pipe");
        close(pipes->pipe_f10_to_f11[0]);
        close(pipes->pipe_f10_to_f11[1]);
        free(pipes);
        return NULL;
    }

    /* Set ALL pipe ends non-blocking */
    fcntl(pipes->pipe_f10_to_f11[0], F_SETFL, O_NONBLOCK);
    fcntl(pipes->pipe_f10_to_f11[1], F_SETFL, O_NONBLOCK);
    fcntl(pipes->pipe_f11_to_f10[0], F_SETFL, O_NONBLOCK);
    fcntl(pipes->pipe_f11_to_f10[1], F_SETFL, O_NONBLOCK);

    return pipes;
}

/*
 * Destroy pipes and cleanup
 */
void ipc_destroy_pipes(pipe_pair_t* pipes) {
    if (!pipes) return;

    close(pipes->pipe_f10_to_f11[0]);
    close(pipes->pipe_f10_to_f11[1]);
    close(pipes->pipe_f11_to_f10[0]);
    close(pipes->pipe_f11_to_f10[1]);
    
    free(pipes);
}

/*
 * Send a message from one intersection to another
 */
int ipc_send_message(pipe_pair_t* pipes, int from_intersection, ipc_message_t* msg) {
    if (!pipes || !msg) return -1;

    int* write_fd = (from_intersection == 0) ? pipes->pipe_f10_to_f11 : pipes->pipe_f11_to_f10;
    
    ssize_t bytes_written = write(write_fd[1], msg, sizeof(ipc_message_t));
    if (bytes_written < 0) {
        /* Handle broken pipe gracefully */
        if (errno == EPIPE || errno == EBADF) {
            return -1;  /* Pipe broken - not fatal */
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;  /* Pipe full - not fatal */
        }
        perror("write");
        return -1;
    }
    return (bytes_written == sizeof(ipc_message_t)) ? 0 : -1;
}

/*
 * Receive a message directed to a specific intersection
 */
int ipc_receive_message(pipe_pair_t* pipes, int to_intersection, ipc_message_t* msg) {
    if (!pipes || !msg) return -1;

    int* read_fd = (to_intersection == 0) ? pipes->pipe_f11_to_f10 : pipes->pipe_f10_to_f11;
    
    ssize_t bytes_read = read(read_fd[0], msg, sizeof(ipc_message_t));
    
    if (bytes_read < 0) {
        /* EAGAIN means no data available - not an error! */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;  /* No message available right now */
        }
        /* Broken pipe or closed - also not fatal */
        if (errno == EPIPE || errno == EBADF) {
            return -1;
        }
        return -1;
    }
    
    if (bytes_read == 0) {
        return -1;  /* EOF - pipe closed */
    }
    
    return (bytes_read == sizeof(ipc_message_t)) ? 0 : -1;
}

/*
 * Send emergency alert to the other intersection
 */
int ipc_send_emergency_alert(pipe_pair_t* pipes, int source, vehicle_t* vehicle) {
    if (!pipes || !vehicle) return -1;

    ipc_message_t msg;
    memset(&msg, 0, sizeof(ipc_message_t));
    
    msg.type = MSG_EMERGENCY_ALERT;
    msg.source_intersection = source;
    msg.vehicle_id = vehicle->id;
    msg.priority = vehicle->priority;
    msg.data = 0;  /* ISSUE #1: Correct dot notation (already correct) */
    
    strncpy(msg.vehicle_type, vehicle->type, sizeof(msg.vehicle_type) - 1);
    msg.vehicle_type[sizeof(msg.vehicle_type) - 1] = '\0';

    /* ISSUE #3: Retry loop for EAGAIN/EWOULDBLOCK to prevent message loss */
    int* write_fd = (source == 0) ? pipes->pipe_f10_to_f11 : pipes->pipe_f11_to_f10;
    int max_retries = 3;
    
    for (int retry = 0; retry < max_retries; retry++) {
        ssize_t bytes_written = write(write_fd[1], &msg, sizeof(ipc_message_t));
        
        if (bytes_written == sizeof(ipc_message_t)) {
            fprintf(stderr, "[IPC] Emergency alert sent (retry %d)\n", retry);
            fflush(stderr);
            return 0;  /* Success */
        }
        
        if (bytes_written < 0) {
            /* Pipe broken or closed - not recoverable */
            if (errno == EPIPE || errno == EBADF) {
                fprintf(stderr, "[IPC] Emergency alert FAILED - pipe broken\n");
                fflush(stderr);
                return -1;
            }
            /* Pipe full - retry with sleep */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (retry < max_retries - 1) {
                    fprintf(stderr, "[IPC] Emergency alert - pipe full, retrying...\n");
                    fflush(stderr);
                    usleep(10000);  /* Sleep 10ms before retry */
                    continue;
                }
            }
            perror("write");
            return -1;
        }
        
        /* Partial write (shouldn't happen) */
        if (retry < max_retries - 1) {
            usleep(10000);
        }
    }
    
    fprintf(stderr, "[IPC] Emergency alert FAILED after 3 retries\n");
    fflush(stderr);
    return -1;
}



