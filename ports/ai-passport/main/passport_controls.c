#include "app_input.h"
#include "passport_input.h"

void passport_dispatch(int action){
    if (action < 0 || action > 5) return;
    appInputDispatch(action % 3, action >= 3);
}
