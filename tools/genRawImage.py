# https://youtu.be/uqY3FMuMuRo?t=1020

from PIL import Image

img    = Image.open( "cam_vga.gif" )
width  = 320
height = 200

# output = open( "rawimage.bin", "wb" )
output = open( "cam_vga.bin", "wb" )

pixels = img.load()

for y in range( height ):

	for x in range( width ):

		pixel = pixels[ x, y ]

		# print( x, y, pixel )

		output.write( bytes( [ pixel ] ) )


img.close()
output.close()
