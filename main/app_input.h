#pragma once
#include <stdbool.h>

// Physical positions after applying the device orientation: left, middle, right.
// Returns the engine's five-action index, or -1 for an unsupported action.
int appInputAction(int button, bool held);
// Reset inactivity and consume input when dismissing a ringing alarm.
bool appInputBegin(void);
void appInputDispatch(int button, bool held);
