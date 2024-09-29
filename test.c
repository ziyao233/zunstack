#include <stdio.h>
#include <string.h>

int
foo(char *a)
{
	a[1000000000] = 0;
	return 0;
}

int
main(void)
{
	int zunstack_init(void);
	zunstack_init();

	char a[20];
	foo(a);
	return 0;
}
