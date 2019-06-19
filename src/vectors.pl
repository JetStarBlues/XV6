#!/usr/bin/perl -w

# Generate vectors.S, the trap/interrupt entry points.
# There has to be one entry point per interrupt number.
# Otherwise there's no way for trap() to discover the
# interrupt number.

print "# Generated by vectors.pl - do not edit\n\n";

print "# There has to be one entry point per interrupt number.\n";
print "# Otherwise there's no way for trap() to discover the\n";
print "# interrupt number.\n";
print "#\n";
print "# For example, the entry point for \"int 5\" is:\n";
print "#\n";
print "# \tvector5:\n";
print "# \t\tpushl \$0         # error number\n";
print "# \t\tpushl \$5         # trap number\n";
print "# \t\tjmp   alltraps   # ISR address\n";
print "\n";

print "# Handlers\n";
print ".globl alltraps\n\n";

for ( my $i = 0; $i < 256; $i++ )
{
	print ".globl vector$i\n";
	print "vector$i:\n";

	# If x86 CPU does not push an error code, push 0
	# https://wiki.osdev.org/Exceptions
	if ( ! ( $i == 8 || ( $i >= 10 && $i <= 14 ) || $i == 17 ) )
	{
		print "\tpushl \$0\n";
	}

	# Push trapno
	print "\tpushl \$$i\n";

	# Go to ISR
	print "\tjmp   alltraps\n\n";
}

print ".data\n\n";

print "\t# Vector table\n";
print "\t.globl vectors\n";
print "\tvectors:\n\n";

for ( my $i = 0; $i < 256; $i++ )
{
	print "\t\t.long vector$i\n";
}

# sample output:
#   # handlers
#   .globl alltraps
#   .globl vector0
#   vector0:
#     pushl $0
#     pushl $0
#     jmp alltraps
#   ...
#   
#   # vector table
#   .data
#   .globl vectors
#   vectors:
#     .long vector0
#     .long vector1
#     .long vector2
#   ...

