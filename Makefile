TARGETS=pbf-parser osm.pb.cc osm.pb.h

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

osm.pb.cc: osm.proto
	protoc --cpp_out=. osm.proto

pbf-parser: main.cpp osm.pb.cc
	g++ -o pbf-parser main.cpp osm.pb.cc `pkg-config --cflags --libs protobuf zlib`
