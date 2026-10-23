// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "learning_model.h"
#include "learning_controls.h"
void learning_platform_controls_get(learning_controls_t *out);
bool learning_platform_controls_set(learning_control_t control,int value);
// Submit must never block the render task. Messages and commands are copied.
bool learning_platform_submit(const learning_command_t *command);
bool learning_platform_poll(learning_message_t *message);
bool learning_platform_key(void);
bool learning_platform_simulated(void);
// Optional platform diagnostics; device implementation performs no work.
void learning_platform_presented(const learning_snapshot_t *snapshot);
