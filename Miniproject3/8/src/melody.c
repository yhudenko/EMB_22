#include "melody.h"

#include <stdbool.h>
#include <stdint.h>

#include "errors.h"

#define C4_HZ 261
#define D4_HZ 293
#define E4_HZ 329
#define F4_HZ 349
#define G4_HZ 392
#define A4_HZ 440
#define B4_HZ 493
#define PAUSE 0

struct note {
  uint32_t frequencyHz;
  uint32_t durationMs;
};

struct melody {
  struct note* notes;
  int noteCount;
};

struct melody_handle {
  enum melody_id id;
  int current_note_index;
  bool is_active;
};

const melody_id_t melody_list[] = {MELODY_GAMA, MELODY_BABY_SHARK,
                                   MELODY_JINGLE_BELLS, MELODY_TWINKLE_TWINKLE,
                                   MELODY_WE_WILL_ROCK_YOU};
const size_t melody_list_size = sizeof(melody_list) / sizeof(melody_list[0]);
const char* melody_names[] = {"Gama", "Baby Shark", "Jingle Bells",
                              "Twinkle Twinkle", "We Will Rock You"};

struct melody gama_melody = {(struct note[]){{C4_HZ, 500},
                                             {D4_HZ, 500},
                                             {E4_HZ, 500},
                                             {F4_HZ, 500},
                                             {G4_HZ, 500},
                                             {A4_HZ, 500},
                                             {B4_HZ, 500}},
                             7};

struct melody baby_shark_melody = {(struct note[]){{C4_HZ, 500},
                                                   {D4_HZ, 500},
                                                   {E4_HZ, 500},
                                                   {PAUSE, 1000},
                                                   {C4_HZ, 500},
                                                   {D4_HZ, 500},
                                                   {E4_HZ, 500},
                                                   {PAUSE, 1000},
                                                   {C4_HZ, 500},
                                                   {D4_HZ, 500},
                                                   {E4_HZ, 500}},
                                   11};

struct melody jingle_bells_melody = {
    (struct note[]){{E4_HZ, 500},  {PAUSE, 50},  {E4_HZ, 500},  {PAUSE, 50},
                    {E4_HZ, 1000}, {PAUSE, 250}, {E4_HZ, 500},  {PAUSE, 50},
                    {E4_HZ, 500},  {PAUSE, 50},  {E4_HZ, 1000}, {PAUSE, 250},
                    {E4_HZ, 500},  {PAUSE, 50},  {G4_HZ, 500},  {PAUSE, 50},
                    {C4_HZ, 500},  {PAUSE, 50},  {D4_HZ, 500},  {PAUSE, 50},
                    {E4_HZ, 1000}},
    21};

struct melody twinkle_twinkle_melody = {(struct note[]){{C4_HZ, 500},
                                                        {PAUSE, 50},
                                                        {C4_HZ, 500},
                                                        {PAUSE, 50},
                                                        {G4_HZ, 500},
                                                        {PAUSE, 50},
                                                        {G4_HZ, 500},
                                                        {PAUSE, 250},
                                                        {A4_HZ, 500},
                                                        {PAUSE, 50},
                                                        {A4_HZ, 500},
                                                        {PAUSE, 50},
                                                        {G4_HZ, 1000}},
                                        13};

struct melody we_will_rock_you_melody = {(struct note[]){{C4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {C4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {D4_HZ, 500},
                                                         {PAUSE, 250},
                                                         {C4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {C4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {D4_HZ, 500},
                                                         {PAUSE, 250},
                                                         {C4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {C4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {D4_HZ, 500},
                                                         {PAUSE, 50},
                                                         {E4_HZ, 500}},
                                         19};

struct melody* melodies[] = {&gama_melody, &baby_shark_melody,
                             &jingle_bells_melody, &twinkle_twinkle_melody,
                             &we_will_rock_you_melody};

struct melody_handle handle;

melody_handle_t handles_pool[] = {&handle};

uint32_t melody_id_to_melody_index(enum melody_id id) {
  return (uint32_t)id - 1;
}

operation_result_t get_melody_by_id(enum melody_id id,
                                    melody_handle_t* handle) {
  if (!handle) {
    return OPERATION_RESULT_ERROR;
  }

  uint32_t melody_index = melody_id_to_melody_index(id);

  if (melody_index >= sizeof(melodies) / sizeof(melodies[0])) {
    return OPERATION_RESULT_ERROR;
  }

  melody_handle_t handle_instance = handles_pool[0];
  if (handle_instance->is_active) {
    return OPERATION_RESULT_ERROR;
  }

  *handle = handle_instance;
  (*handle)->id = id;
  (*handle)->current_note_index = 0;
  (*handle)->is_active = true;

  return OPERATION_RESULT_OK;
}

operation_result_t get_next_note_params(melody_handle_t melody_handle,
                                        uint32_t* frequency,
                                        uint32_t* duration) {
  if (!melody_handle || !frequency || !duration || !melody_handle->is_active) {
    return OPERATION_RESULT_ERROR;
  }

  uint32_t melody_index = melody_id_to_melody_index(melody_handle->id);
  if (melody_index >= sizeof(melodies) / sizeof(melodies[0])) {
    return OPERATION_RESULT_ERROR;
  }

  struct melody* melody = melodies[melody_index];
  if (melody_handle->current_note_index >= melody->noteCount) {
    return OPERATION_RESULT_ERROR;
  }

  struct note* n = &melody->notes[melody_handle->current_note_index];
  *frequency = n->frequencyHz;
  *duration = n->durationMs;
  melody_handle->current_note_index++;

  return OPERATION_RESULT_OK;
}

operation_result_t release_melody(melody_handle_t handle) {
  if (!handle || !handle->is_active) {
    return OPERATION_RESULT_ERROR;
  }

  handle->is_active = false;
  handle->current_note_index = 0;
  handle->id = NONE;

  return OPERATION_RESULT_OK;
}