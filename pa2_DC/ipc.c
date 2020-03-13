#define _XOPEN_SOURCE 600 /* needed for timespec in <time.h> */
#include "ipc.h"
#include "utils.h"
#include <stdbool.h>
#include "log.h"
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "pa2345.h"
#include "log.h"
#include "banking.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

int send(void * self, local_id dst, const Message * msg) {
    Process* current_process = self;
    write(writer[current_process->id][dst], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    Process* current_process = self;
    int bytes_read;
    while(true){
    bytes_read = read(reader[from][current_process->id], &msg->s_header, sizeof(MessageHeader));\
        if (bytes_read <= 0) {
            continue;
        }
            if (msg->s_header.s_payload_len > 0){
                do {
                    bytes_read = read(reader[from][current_process->id], &msg->s_payload, msg->s_header.s_payload_len);
                } while (bytes_read <= 0);
            }
        return 0;
    }
}

int receive_from_all_children(Process* self, Message* msg){
    for (int i = 1; i <= num_children; i++) {
        if (i != self->id){
            receive(self, i, msg);
            // printf("------Process %d received type %d\n", self->id, msg->s_header.s_type);
        }
    }
    return 0;
}

/**
 * Sends message to ALL processes
 */ 
int send_multicast(void * self, const Message * msg) {
    //This is done to cast void pointer to Process. Because we cannot modify ipc.h
    Process* current_process = self;
    for (int i = 0; i < num_processes; i++) {
        if (i != current_process->id)
            send(current_process, i, msg);
    }
    return 0;
}

int send_started_to_all(Process* self) {
    Message msg = {
        .s_header =
            {
                .s_magic = MESSAGE_MAGIC,
                .s_type = STARTED,
            },
    };
    int payload_len = sprintf(msg.s_payload, log_started_fmt, get_physical_time(), self->id, getpid(), getppid(), self->history.s_history->s_balance);
    msg.s_header.s_payload_len = payload_len;
    return send_multicast(self, &msg);
}

int send_done_to_all(Process* self) {
    Message msg = {
        .s_header =
            {
                .s_magic = MESSAGE_MAGIC,
                .s_type = DONE,
            },
    };
    int payload_len = sprintf(msg.s_payload, log_done_fmt, get_physical_time(), self->id, self->history.s_history->s_balance);
    msg.s_header.s_payload_len = payload_len;
    return send_multicast(self, &msg);
}

int send_stop_to_all(Process* self) {
    Message msg = {
        .s_header =
            {
                .s_magic = MESSAGE_MAGIC,
                .s_type = STOP,
                .s_payload_len = 0,
            },
    };
    return send_multicast(self, &msg);
}

int send_history(Process* self) {

    size_t size_of_history = sizeof(local_id) +
                             sizeof(uint8_t) +
                             self->history.s_history_len * sizeof(BalanceState);
    Message msg = {
        .s_header = {
            .s_magic = MESSAGE_MAGIC,
            .s_type = BALANCE_HISTORY,
            .s_local_time = self->lamport_time,
            .s_payload_len = size_of_history,
        }
    };
    memcpy(&msg.s_payload, &self->history, size_of_history);
    return send(self, PARENT_ID, &msg);
}

int receive_any(void * self, Message * msg) {
    Process* current_process = self;
    while(true) {
        int bytes_read;
        for (int from = 0; from < num_processes; from++) {
            if (from == current_process->id)
                continue;
            bytes_read = read(reader[from][current_process->id], &msg->s_header, sizeof(MessageHeader));
            if (bytes_read <= 0) {
                continue;
            }
            if (msg->s_header.s_payload_len > 0){
                do {
                    bytes_read =read(reader[from][current_process->id], &msg->s_payload, msg->s_header.s_payload_len);
                } while (bytes_read <= 0);
            }
            return 0;
        }
    }
}
