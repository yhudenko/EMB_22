#ifndef MELODY_H
#define MELODY_H

#include <stddef.h>
#include <stdint.h>

#include "errors.h"

enum melody_id {
  NONE,
  MELODY_GAMA,
  MELODY_BABY_SHARK,
  MELODY_JINGLE_BELLS,
  MELODY_TWINKLE_TWINKLE,
  MELODY_WE_WILL_ROCK_YOU
};
typedef enum melody_id melody_id_t;

struct melody_handle;

typedef struct melody_handle* melody_handle_t;

extern const melody_id_t melody_list[];
extern const size_t melody_list_size;
extern const char* melody_names[];

operation_result_t get_melody_by_id(melody_id_t id, melody_handle_t* handle);
operation_result_t get_next_note_params(melody_handle_t melody,
                                        uint32_t* frequency,
                                        uint32_t* duration);
operation_result_t release_melody(melody_handle_t handle);

#endif  // MELODY_H