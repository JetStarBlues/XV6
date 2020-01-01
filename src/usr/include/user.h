#include "stdarg.h"  // GCC builtin

struct stat;
struct rtcdate;

// system calls
int   chdir   ( const char* );
int   close   ( int );
int   dup     ( int );
int   exec    ( char*, char* [] );
int   exit    ( void ) __attribute__( ( noreturn ) );
int   fork    ( void );
int   fstat   ( int, struct stat* );
int   getpid  ( void );
int   gettime ( struct rtcdate* );
int   ioctl   ( int, int, ... );
int   kill    ( int );
int   link    ( const char*, const char* );
int   lseek   ( int, int, int );
int   mkdir   ( const char* );
int   mknod   ( const char*, short, short );
int   open    ( const char*, int );
int   pipe    ( int* );
int   read    ( int, void*, int );
char* sbrk    ( int );
int   sleep   ( int );
int   unlink  ( const char* );
int   uptime  ( void );
int   wait    ( void );
int   write   ( int, const void*, int );

// printf.c
int printf    ( int, const char*, ... );
int snprintf  ( char*, int, const char*, ... );
int sprintf   ( char*, const char*, ... );
int vprintf   ( int, const char*, va_list );
int vsnprintf ( char*, int, const char*, va_list );
int vsprintf  ( char*, const char*, va_list );

// ulib.c
int   atoi    ( const char* );
int   getc    ( int );
int   getline ( char**, uint*, int );
char* gets    ( char*, int );
void* memcpy  ( void*, const void*, uint );
void* memmove ( void*, const void*, uint );
void* memset  ( void*, int, uint );
// int   putc    ( int, int );
int   stat    ( const char*, struct stat* );
char* strchr  ( const char*, char );
int   strcmp  ( const char*, const char* );
char* strcpy  ( char*, const char* );
char* strdup  ( const char* );
uint  strlen  ( const char* );
int   strncmp ( const char*, const char*, int );
char* strncpy ( char*, const char*, int );

// umalloc.c
void  free    ( void* );
void* malloc  ( uint );
void* realloc ( void*, uint );


// ctype...
#define ISDIGIT( c ) ( ( ( c ) >= '0' ) && ( ( c ) <= '9' ) )


/* Chuck these here for now, since almost everything
   includes 'user.h'
*/
#define NULL   0

#define stdin  0
#define stdout 1
#define stderr 2
