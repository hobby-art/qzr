#ifndef UI_H
#define UI_H


// screen & cursor
#define CSR_HOME "\x1b[H"
#define SCR_ERASE "\x1b[2J"
#define ERASE_AND_HOME "\x1b[2J\x1b[H"


// text modes
#define RESET "\x1b[0m"

#define BOLD "\x1b[1m"
#define BOLD_R "\x1b[22m"

#define UNDERLINE "\x1b[4m"
#define UNDERLINE_R "\x1b[24m"

#define BLINKING "\x1b[5m"
#define BLINKING_R "\x1b[25m"

#define INVERSE "\x1b[7m"


// text colors
#define RED "\x1b[31m"
#define RED_BG "\x1b[41m"

#define GREEN "\x1b[32m"
#define GREEN_BG "\x1b[42m"

#define CYAN "\x1b[36m"
#define CYAN_BG "\x1b[46m"


#endif
