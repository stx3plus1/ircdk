#include <termios.h>

int main() {	
	struct termios term;
	tcgetattr(0, &term);
    term.c_lflag |= (ECHO | ICANON);
    tcsetattr(0, TCSAFLUSH, &term);

	return 0;
}
