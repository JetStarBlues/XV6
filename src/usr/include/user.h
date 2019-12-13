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
int   ioctl   ( int, int, uint* );
int   kill    ( int );
int   link    ( const char*, const char* );
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

// ulib.c
int   atoi    ( const char* );
char* gets    ( char*, int );
void* memcpy  ( void*, const void*, uint );
void* memmove ( void*, const void*, uint );
void* memset  ( void*, int, uint );
void  printf  ( int, const char*, ... );
int   stat    ( const char*, struct stat* );
char* strchr  ( const char*, char );
int   strcmp  ( const char*, const char* );
char* strcpy  ( char*, const char* );
uint  strlen  ( const char* );

// umalloc.c
void  free    ( void* );
void* malloc  ( uint );
void* realloc ( void*, uint );


// ctype...
#define ISDIGIT( c ) ( ( ( c ) >= '0' ) && ( ( c ) <= '9' ) )
