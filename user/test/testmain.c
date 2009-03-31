/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include <esc/common.h>
#include <esc/fileio.h>
#include <esc/debug.h>
#include <esc/io.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	tFD fd;
	u8 buffer[1024];

	fd = open("file:/zeros",IO_READ);
	if(fd < 0) {
		printe("Unable to open file:/zeros");
		return EXIT_FAILURE;
	}

	dbg_startTimer();
	while(read(fd,buffer,1024) > 0);
	dbg_stopTimer("Reading took ");

	close(fd);

	/*int c1,c2,c3;
	char line[50];
	char str[] = "- This, a sample string.";
	char *pch;
	s32 res;

	printf("Splitting string \"%s\" into tokens:\n",str);
	pch = strtok(str," ,.-");
	while(pch != NULL) {
		printf("'%s'\n",pch);
		pch = strtok(NULL," ,.-");
	}

	printf("str=%p,%n pch=%p,%n abc=%p,%n\n",str,&c1,pch,&c2,0x1234,&c3);
	printf("c1=%d, c2=%d, c3=%d\n",c1,c2,c3);

	printf("num1: '%-8d', num2: '%8d', num3='%-16x', num4='%-012X'\n",
			100,200,0x1bca,0x12345678);

	printf("num1: '%-+4d', num2: '%-+4d'\n",-100,50);
	printf("num1: '%- 4d', num2: '%- 4d'\n",-100,50);
	printf("num1: '%#4x', num2: '%#4X', num3 = '%#4o'\n",0x123,0x456,0377);
	printf("Var padding: %*d\n",8,-123);
	printf("short: %4hx\n",0x1234);

	while(1) {
		printf("Now lets execute something...\n");
		scanl(line,50);
		res = system(line);
		printf("Result: %d\n",res);
	}*/

	return EXIT_SUCCESS;
}
