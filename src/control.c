#include <assert.h>

#include "control.h"
#include "utils.h"

User_Input_Type get_user_input(char *c) {
    assert(c);
    while (1) {
        *c = get_char();
        if (ESCSEQ(*c)) {
            int next0 = get_char();
            if (CSI(next0)) {
                int next1 = get_char();
                switch (next1) {
                case DOWN_ARROW:
                case RIGHT_ARROW:
                case LEFT_ARROW:
                case UP_ARROW:
                    *c = next1;
                    return USER_INPUT_TYPE_ARROW;
                default:
                    return USER_INPUT_TYPE_UNKNOWN;
                }
            } else { // [ALT] key
                *c = next0;
                return USER_INPUT_TYPE_ALT;
            }
        }
        else if (*c == CTRL_N) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_P) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_G) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_D) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_U) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_V) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_W) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_O) return USER_INPUT_TYPE_CTRL;
        else if (*c == CTRL_L) return USER_INPUT_TYPE_CTRL;
        else return USER_INPUT_TYPE_NORMAL;
    }
    return USER_INPUT_TYPE_UNKNOWN;
}
