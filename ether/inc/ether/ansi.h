#ifndef ETHER_ANSI_H
#define ETHER_ANSI_H

/* C0 Control Codes */
#define CO_BELL "\a"
#define CO_BACKSPACE "\b"
#define CO_TAB "\t"
#define CO_LINE_FEED "\n"
#define CO_FORM_FEED "\f"
#define CO_CARRIAGE_RETURN "\r"
#define CO_ESCAPE "\x1b"

/* Control Sequence Introducer Commands */
#define CSI CO_ESCAPE "["
#define CSI_CURSOR_UP CSI "A"
#define CSI_CURSOR_UP_N CSI "%dA"
#define CSI_CURSOR_DOWN CSI "B"
#define CSI_CURSOR_DOWN_N CSI "%dB"
#define CSI_CURSOR_FORWARD CSI "C"
#define CSI_CURSOR_FORWARD_N CSI "%dC"
#define CSI_CURSOR_BACK CSI "D"
#define CSI_CURSOR_BACK_N CSI "%dD"

#define CSI_CURSOR_NEXT_LINE CSI "E"
#define CSI_CURSOR_NEXT_LINE_N CSI "%dE"
#define CSI_CURSOR_PREVIOUS_LINE CSI "F"
#define CSI_CURSOR_PREVIOUS_LINE_N CSI "%dF"

#define CSI_CURSOR_HORIZONTAL_ABSOLUTE CSI "%dG"

#define CSI_CURSOR_POSITION CSI "%d;%dH"

#define ERASE_TO_END "0"
#define ERASE_TO_BEGINNING "1"
#define ERASE_ENTIRE "2"
#define ERASE_ENTIRE_AND_HISTORY "3"

#define CSI_ERASE_DISPLAY(mode) CSI mode "J"

#define CSI_ERASE_LINE(mode) CSI mode "K"

#define CSI_SCROLL_UP CSI "%dS"
#define CSI_SCROLL_DOWN CSI "%dT"

#define CSI_SGR(n) CSI #n "m"

/// @brief Reports the cursor position (CPR) by transmitting ESC[n;mR, where n is the row and m is the column.
#define CSI_DEVICE_STATUS_REPORT CSI "6n"

#define CSI_SAVE_CURSOR CSI "s"
#define CSI_RESTORE_CURSOR CSI "u"

#define CSI_SHOW_CURSOR CSI "?25h"
#define CSI_HIDE_CURSOR CSI "?25l"

#define CSI_ENABLE_ALTERNATE_BUFFER CSI "?1049h"
#define CSI_DISABLE_ALTERNATE_BUFFER CSI "?1049l"

/* Select Graphic Rendition Parameters */

#define SGR_RESET CSI_SGR(0)
#define SGR_BOLD CSI_SGR(1)
#define SGR_FAINT CSI_SGR(2)
#define SGR_ITALIC CSI_SGR(3)
#define SGR_UNDERLINE CSI_SGR(4)
#define SGR_SLOW_BLINK CSI_SGR(5)
#define SGR_RAPID_BLINK CSI_SGR(6)
#define SGR_INVERT CSI_SGR(7)
#define SGR_CONCEAL CSI_SGR(8)
#define SGR_STRIKE CSI_SGR(9)

#define SGR_DOUBLE_UNDERLINE CSI_SGR(21)

#define SGR_REVEAL CSI_SGR(28)

#define BLACK "0"
#define RED "1"
#define GREEN "2"
#define YELLOW "3"
#define BLUE "4"
#define MAGENTA "5"
#define CYAN "6"
#define WHITE "7"

#define SGR_SET_FG_4BIT(color) CSI "3" #color "m"
#define SGR_SET_FG_4BIT_BRIGHT(color) CSI "9" #color "m"
#define SGR_SET_BG_4BIT(color) CSI "4" #color "m"
#define SGR_SET_BG_4BIT_BRIGHT(color) CSI "10" #color "m"

#define SGR_SET_FG_8BIT(n) CSI "38;5;" #n "m"
#define SGR_SET_BG_8BIT(n) CSI "48;5;" #n "m"

#define SGR_SET_FG_24BIT CSI "38;2;%d;%d;%dm"
#define SGR_SET_BG_24BIT CSI "48;2;%d;%d;%dm"

#endif