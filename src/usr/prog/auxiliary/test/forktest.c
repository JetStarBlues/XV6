// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "kernel/types.h"
#include "user.h"

#define N 1000

static void _printf ( int fd, const char* s, ... )
{
	write( fd, s, strlen( s ) );
}

void forktest ( void )
{
	int n, pid;

	_printf( stdout, "fork test\n" );

	for ( n = 0; n < N; n += 1 )
	{
		pid = fork();

		if ( pid < 0 )
		{
			break;
		}

		if ( pid == 0 )
		{
			exit();
		}
	}

	if ( n == N )
	{
		_printf( stdout, "fork test: fork claimed to work %d times!\n", N );

		exit();
	}

	for ( ; n > 0; n -= 1 )
	{
		if ( wait() < 0 )
		{
			_printf( stdout, "fork test: wait stopped early\n" );

			exit();
		}
	}

	if ( wait() != - 1 )
	{
		_printf( stdout, "fork test: wait got too many\n" );

		exit();
	}

	_printf( stdout, "fork test: OK\n" );
}

int main ( void )
{
	forktest();

	exit();
}
