#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(int argc, char *argv[])
{
	char c=14;
	int a=10;
	while (a>0)
	{
		printf("%d",a--);
	}
	printf("\n\n\n>>>%d<<<\n", c/a);
	return 0;
}
