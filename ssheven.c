/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

// retro68 stdio/console library
#include <stdio.h>

// open transport
#include <OpenTransport.h>
#include <OpenTptInternet.h>

// mac os threads
#include <Threads.h>

int main(int argc, char** argv)
{
	printf("hello, world\n");

	printf("\n(return to exit)\n");
	getchar();

	return 0;
}

