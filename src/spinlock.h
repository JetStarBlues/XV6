#define SPLKNPCS 10

// Mutual exclusion lock
struct spinlock {

	uint        locked;            // Is the lock held?

	// For debugging:
	char*       name;              // Name of lock
	struct cpu* cpu;               // The cpu holding the lock
	uint        pcs [ SPLKNPCS ];  /* The call stack used to acquired the lock
	                                  (an array of program counters) */
};
