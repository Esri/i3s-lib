# raster2slpk example application

The application accepts two images as input: an elevation bitmap and a color bitmap, and produces an SLPK representing the terrain surface defined by these two bitmaps. The current implementation is very simplistic and works with particular input data obtained [from here](https://www.cc.gatech.edu/projects/large_models/ps.html).

The app works under the following assumptions about the input data:
* color bitmap has equal width and height
* width/height of the color bitmap is a power of 2
* elevation bitmap width and height are equal to the color bitmap width/height plus one (i.e. elevation values are defined on vertices of a grid while color values are defined on cells of this grid)

The output i3s tree has the following structure:
* every node represents a square patch of the surface
* every internal node has the same number of triangles and the same texture dimensions
(e.g. 32 * 32 * 2 triangles, the texture of 128 * 128 pixels)
* every internal node has 4 children representing 2 * 2 subdivision of the node area

The above requirements are quite restrictive, but allow for pretty simple and fast implementation.

To run the application, you need to specify six parameters:
* elevation image (must be a grayscale PNG file)
* color image (must be a PNG file)
* output file (SLPK) path
* x (lat) resolution of the elevation and color grids (meters / pixel)
* y (lon) resolution of the elevation and color grids (meters / pixel)
* elevation unit (in meters)

For the images we use, elevation unit is 0.1 m, x and y resolution is 10 m/p for the highest resolution images, 40 m/p for the medium, and 160 m/p for the ones with the lowest resolution.
