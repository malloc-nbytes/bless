#ifndef CONTROL_H
#define CONTROL_H

#define CTRL_N 14 // Scroll down
#define CTRL_D 4  // Page down
#define CTRL_U 21 // Page up
#define CTRL_L 12 // Refresh
#define CTRL_P 16 // Scroll up
#define CTRL_G 7  // Cancel
#define CTRL_V 22 // Scroll down
#define CTRL_W 23 // Save buffer
#define CTRL_O 15 // Open buffer
//#define CTRL_I 9  // Open editor

#define UP_ARROW      'A'
#define DOWN_ARROW    'B'
#define RIGHT_ARROW   'C'
#define LEFT_ARROW    'D'

#define ENTER(ch)     (ch) == '\n'
#define BACKSPACE(ch) (ch) == 8 || (ch) == 127
#define ESCSEQ(ch)    (ch) == 27
#define CSI(ch)       (ch) == '['
#define TAB(ch)       (ch) == '\t'

typedef enum {
    USER_INPUT_TYPE_CTRL,
    USER_INPUT_TYPE_ALT,
    USER_INPUT_TYPE_ARROW,
    USER_INPUT_TYPE_NORMAL,
    USER_INPUT_TYPE_UNKNOWN,
} User_Input_Type;

User_Input_Type get_user_input(char *c);

#endif // CONTROL_H
