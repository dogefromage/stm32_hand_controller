#pragma once

typedef struct host_to_hand_command_t {
    int16_t actuators[16];
} host_to_hand_command_t;

typedef struct hand_to_host_commant_t {
    int16_t sensors[32];
} hand_to_host_commant_t;
