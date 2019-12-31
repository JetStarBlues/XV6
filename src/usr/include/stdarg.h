// From: Compiler Design in C, Allen Holub, p.724

typedef char* va_list;

#define va_start( arg_ptr, first )                      \
	                                                    \
	arg_ptr = ( va_list ) ( &first ) + sizeof( first )

#define va_arg( arg_ptr, type )                         \
	                                                    \
	( ( type* ) ( arg_ptr += sizeof( type ) ) )[ - 1 ]

#define va_end( arg_ptr )  /* empty */


/* Example output:

	void printInt ( int argCount, ... )
	{
		// va_list args;
		char* args;

		// va_start( args, argCount );
		args = ( char* ) ( &argCount ) + sizeof( argCount );

		while ( argCount > 0 )
		{
			printf(

				"%d ",
				// va_arg( args, int )
				( ( int* ) ( args += sizeof( int ) ) )[ - 1 ]
			);

			argCount -= 1;
		}

		// va_end( args );
	}
*/
