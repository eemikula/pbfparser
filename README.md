This is a simple example of parsing PBF files from OpenStreetMap.org in C++.

Since PBF files use Google's protobuf specification, but at the same time
cannot be read from directly with the protobuf API such as with
ParseFromIstream, it takes some digging through documentation and source code
in order to be able to read from a file correctly.

If you have any need to read a PBF file without the use of other tools like
osmosis or imposm-parser, the hope is that this might serve as a useful
reference proof-of-concept.

This example works by reading the contents of PBF files from a file stream
using only the bare minimum dependencies, zlib and protobuf, in order to read
through a specified file and output the IDs and names of any nodes, ways, or
relations that have names and are listed as being pubs, restaurants, or bars.

