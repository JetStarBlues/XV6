'''
Generate an image to test a 256 color palette.

The image is a grid, with each square holding a
color index (0..255).

Because (200 % 16) = 8, there is an extra (half) row.
We fill this row with color 0

The grid dimensions are:
	. 16   columns
	. 16.5 rows
'''

width  = 320
height = 200

img = [ 0 ] * ( width * height )

cellwidth  = width  // 16
cellheight = height // 16

def gen ():

	colorIdx = 0
	imgIdx   = 0

	for y in range( 0, height, cellheight ):

		for x in range( 0, width, cellwidth ):

			for xx in range( cellwidth ):

				for yy in range( cellheight ):

					imgIdx = width * ( y + yy ) + ( x + xx )

					img[ imgIdx ] = colorIdx

					# print( colorIdx, x, y, xx, yy, imgIdx )

			colorIdx += 1

			if colorIdx == 256:

				return

gen()

output = open( "paltest.bin", "wb" )

output.write( bytes( img ) )

output.close()


def visualize ():

	debug = open( "debug.txt", "w" )

	for y in range( height ):
	
		for x in range( width ):

			idx = width * y + x

			c = img[ idx ]

			ch = hex( c )[ 2: ].zfill( 2 )

			# print( ch, end = "" )
			debug.write( ch )

		# print( "" )
		debug.write( '\n' );

	debug.close()

visualize()
