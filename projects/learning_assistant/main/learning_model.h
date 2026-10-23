// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "learning_questions.h"

enum { LEARN_TEXT_CAP = 8192, LEARN_LINE_CAP = 97, LEARN_LINES = 6,
       LEARN_VOICE_TEXT_CAP = 1024, LEARN_MAX_PAGES = 128, LEARN_MAX_RECORD_MS = 30000 };
_Static_assert(sizeof(learning_questions_t)<=LEARN_TEXT_CAP,"Question payload must fit the existing message budget");
typedef struct learning_model_t *learning_model_handle_t;
typedef enum { LEARN_HOME, LEARN_CAMERA, LEARN_CONFIRM, LEARN_READING,
    LEARN_VOICE, LEARN_WAITING, LEARN_SETTINGS, LEARN_ERROR, LEARN_VOICE_REVIEW, LEARN_QUESTIONS } learning_page_t;
typedef enum { LEARN_PHOTO, LEARN_CAPTURE, LEARN_RETAKE, LEARN_SEND,
    LEARN_ASK, LEARN_KEY_DOWN, LEARN_KEY_UP, LEARN_BACK, LEARN_PREVIOUS,
    LEARN_NEXT, LEARN_EXPLAIN, LEARN_READ, LEARN_MUTE, LEARN_SETUP,
    LEARN_OPEN_SETTINGS, LEARN_CANCEL, LEARN_RETRY, LEARN_VOICE_TOGGLE, LEARN_SEND_TEXT,
    LEARN_LIGHT,LEARN_FLIP_CAMERA, LEARN_SCAN, LEARN_PICK_0, LEARN_PICK_1, LEARN_PICK_2, LEARN_CHOOSE_AGAIN } learning_action_t;
typedef enum { LEARN_CMD_NONE, LEARN_CMD_CAMERA, LEARN_CMD_CAPTURE,
    LEARN_CMD_SOLVE, LEARN_CMD_RECORD, LEARN_CMD_SEND_AUDIO,
    LEARN_CMD_CANCEL, LEARN_CMD_EXPLAIN, LEARN_CMD_SPEAK, LEARN_CMD_SETUP,
    LEARN_CMD_MUTE, LEARN_CMD_SEND_TEXT,LEARN_CMD_CAMERA_OPTIONS, LEARN_CMD_IDENTIFY } learning_command_kind_t;
typedef struct { learning_command_kind_t kind; uint32_t generation;
    bool muted,followup,light,flipped,keep_photo; char text[LEARN_VOICE_TEXT_CAP]; } learning_command_t;
typedef enum { LEARN_API_UNKNOWN, LEARN_API_READY, LEARN_API_ERROR } learning_api_state_t;

typedef enum { LEARN_MSG_PHOTO, LEARN_MSG_ANSWER, LEARN_MSG_TRANSCRIPT,
    LEARN_MSG_ERROR, LEARN_MSG_SPEECH_DONE, LEARN_MSG_STATUS, LEARN_MSG_SETUP,LEARN_MSG_CAMERA_OPTIONS, LEARN_MSG_QUESTIONS } learning_message_kind_t;
typedef struct { learning_message_kind_t kind; uint32_t generation;
    bool online, configured, speaking,light,flipped; learning_api_state_t api_state;
    // Mutually exclusive payloads keep the camera-era queue memory budget.
    union { char text[LEARN_TEXT_CAP]; learning_questions_t questions; };
} learning_message_t;
typedef struct { learning_page_t page; uint32_t generation, revision;
    bool online, configured, muted, recording, speaking, photo_ready;
    unsigned page_index, page_count, recording_ms;
    learning_api_state_t api_state;
    char transcript[LEARN_VOICE_TEXT_CAP];
    unsigned review_index,review_count;
    bool tap_recording,light,flipped;
    unsigned question_count,question_page,selected_question;
    learning_question_t question_rows[LEARN_QUESTION_ROWS];
    char lines[LEARN_LINES][LEARN_LINE_CAP];
    char setup_lines[3][LEARN_LINE_CAP];
    char hint[192]; } learning_snapshot_t;

// The UI task alone owns the model. Drivers exchange copies through queues.
learning_model_handle_t learning_model_create(bool simulated);
void learning_model_delete(learning_model_handle_t model);
bool learning_model_action(learning_model_handle_t model, learning_action_t action,
                           learning_command_t *command);
void learning_model_message(learning_model_handle_t model, const learning_message_t *message);
bool learning_model_tick(learning_model_handle_t model, unsigned elapsed_ms,
                         learning_command_t *command);
void learning_model_snapshot(learning_model_handle_t model, learning_snapshot_t *snapshot);
