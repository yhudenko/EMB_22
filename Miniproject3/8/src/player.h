#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

#include "errors.h"
#include "melody.h"

typedef int* player_handle_t;

operation_result_t initialize_player(player_handle_t* handle);
operation_result_t play_melody(player_handle_t player, melody_handle_t melody);
operation_result_t stop_melody(player_handle_t player, melody_handle_t melody);
operation_result_t release_player(player_handle_t player);
bool is_melody_playing(player_handle_t player, melody_handle_t melody);

#endif  // PLAYER_H