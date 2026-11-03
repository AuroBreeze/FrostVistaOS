#include <user/user.h>

extern int main(int argc, char **argv);

/* Architecture entry code prepares registers before entering the C runtime. */
void __rime_start(int argc, char **argv)
{
	exit(main(argc, argv));
}
