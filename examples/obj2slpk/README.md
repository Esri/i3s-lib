# obj2slpk sample application

This application is designed to act as a template for those looking to convert OBJ tiles
typically generated as an output for an SFM process for digital twin style models.

This app is under active development and details may change.

The following assumptions are made about the input data:
* The OBJ file is an ENU co-ordinate system
* The full resolution OBJ and pre-computed LOD's are available
* The co-ordinate system details and offsets are stored in an XML sidecar file

The i3s output does not employ a quad-tile mechanism (this may be implemented in the future).
It packs all the different LOD obj files and textures into a single SLPK. In the planned
quadtile implementation the full-resolution OBJ and larger LOD's may be split into quadtiles/
octiles and a HLOD created.

The basic implementation also does not account for input of multiple tiles. It is planned that
higher level nodes will be created to span multiple tiles adjacent to each other to provide
reasonable load performance for most viewers.

To run the application, you need to specify five parameters:
* Full resolution OBJ
* LOD1 OBJ
* LOD2 OBJ
* Co-ordinate system XML
* Output SLPK

It is planned that a more advanced parameter parser like [getopt](https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html) can be used to read a larger number of input LOD levels.