//
// Created by bytebard on 10/26/25.
//

#include "renderer.h"

std::pair<int, int> get_cursor_position() {
    struct termios old_t, new_t;
    tcgetattr(STDOUT_FILENO, &old_t);
    new_t = old_t;

    new_t.c_lflag = new_t.c_lflag & ~(ICANON | ECHO);
    tcsetattr(STDOUT_FILENO, TCSANOW, &new_t);

    std::cout << "\033[6n" << std::flush;

    char buf[32];
    int i = 0;
    while (i < 31) {
        if (read(STDOUT_FILENO, &buf[i], 1) < 0) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_t);
    int row, col;

    auto res = std::sscanf(buf, "\033[%d;%dR", &row, &col);
    if (res == 2) {
        return { row, col };
    }
    return { 1, 1 };
}
