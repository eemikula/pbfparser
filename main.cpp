/*
The MIT License (MIT)

Copyright (c) 2016 Eric Mikula

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include "osm.pb.h"
#include <netinet/in.h>
#include <zlib.h>
#include <stdint.h>

std::fstream &readDataStr(std::fstream &in, std::string &str, size_t size){
	char *data = new char[size];
	in.read(data, size);
	str.assign(data, size);
	delete[] data;
	return in;
}

std::fstream &readBlob(std::fstream &in, OSMPBF::Blob &blob){
	OSMPBF::BlobHeader blobHeader;
	unsigned int blobHeaderSize;
	in.read((char*)&blobHeaderSize, sizeof(blobHeaderSize));
	blobHeaderSize = ntohl(blobHeaderSize);

	try {
		std::string data;
		if (!readDataStr(in, data, blobHeaderSize)){
			in.setstate(std::ios_base::badbit);
			return in;
		}

		if (!blobHeader.ParseFromString(data))
			throw "Failed to parse from string";
		if (!readDataStr(in, data, blobHeader.datasize()))
			throw "Unable to read data";
		if (!blob.ParseFromString(data))
			throw "Failed to parse";

	} catch (const char *s){
		std::cerr << s << "\n";
		in.setstate(std::ios_base::badbit);
		return in;
	}

	return in;
}

bool inflate(std::string data, unsigned char *buf, size_t bufSz){
	z_stream zstrm;
	zstrm.zalloc = Z_NULL;
	zstrm.zfree = Z_NULL;
	zstrm.opaque = Z_NULL;
	zstrm.avail_in = 0;
	zstrm.next_in = Z_NULL;
	if (inflateInit(&zstrm) != Z_OK){
		std::cerr << "Unable to initialize zlib\n";
		return false;
	}

	zstrm.avail_in = data.capacity();
	zstrm.next_in = (Bytef*)data.data();
	zstrm.avail_out = bufSz;
	zstrm.next_out = buf;
	int r = inflate(&zstrm, Z_NO_FLUSH);
	if (r == Z_STREAM_ERROR){
		std::cout << "Not OK: " << r << "\n";
		return false;
	}

	inflateEnd(&zstrm);
	return true;
}

template <typename T>
bool getCompressedBlock(OSMPBF::Blob &blob, T &block){
	try {
		if (blob.has_zlib_data()){

			unsigned char *buf = new unsigned char[blob.raw_size()];
			if (!inflate(blob.zlib_data(), buf, blob.raw_size()))
				throw "Unable to decompress zlib data";

			std::string datastr((char*)buf, blob.raw_size());
			if (!block.ParseFromString(datastr)){
				throw "Cannot parse block";
			}
			delete[] buf;
		}
	} catch (const char *s){
		std::cerr << s << "\n";
		return false;
	}

	return true;
}

std::fstream &readHeaderBlock(std::fstream &in, OSMPBF::HeaderBlock &headerBlock){

	OSMPBF::Blob blob;
	if (!readBlob(in, blob))
		return in;

	if (!getCompressedBlock(blob, headerBlock))
		in.setstate(std::ios_base::badbit);
	else {
		std::cout << "Required features:\n";
		for (int i = 0; i < headerBlock.required_features_size(); i++){
			std::cout << "\t" << headerBlock.required_features(i) << "\n";
		}
	}

	return in;

}

int main(int argc, char *argv[]){

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	if (argc != 2){
		std::cerr << "Please specify pbf file\n";
		return 1;
	}

	std::fstream input(argv[1], std::ios::in | std::ios::binary);
	if (!input){
		std::cerr << "Can't open " << argv[1] << "\n";
		return 1;
	}

	OSMPBF::HeaderBlock fileHeader;
	if (!readHeaderBlock(input, fileHeader)){
		std::cerr << "Unable to read header block\n";
		return 1;
	}

	OSMPBF::Blob blob;
	while (input){
		readBlob(input, blob);

		OSMPBF::PrimitiveBlock block;
		if (getCompressedBlock(blob, block)){

			for (int g = 0; g < block.primitivegroup_size(); g++){

				for (int w = 0; w < block.primitivegroup(g).ways_size(); w++){
					const OSMPBF::Way &way = block.primitivegroup(g).ways(w);
					const std::string *name = 0;
					bool pub = false;
					for (int key = 0; key < way.keys_size(); key++){
						unsigned int k = way.keys(key);
						unsigned int v = way.vals(key);

						if (block.stringtable().s(k) == "amenity" &&
							(block.stringtable().s(v) == "pub" || block.stringtable().s(v) == "restaurant" || block.stringtable().s(v) == "bar")){
							pub = true;
						}
						else if (block.stringtable().s(k) == "name")
							name = &block.stringtable().s(v);
					}
					if (pub && name)
						std::cout << way.id() << " " << *name << "\n";
				}
				for (int n = 0; n < block.primitivegroup(g).nodes_size(); n++){
					const OSMPBF::Node &node = block.primitivegroup(g).nodes(n);
					const std::string *name = 0;
					bool pub = false;
					for (int key = 0; key < node.keys_size(); key++){
						unsigned int k = node.keys(key);
						unsigned int v = node.vals(key);
						if (block.stringtable().s(k) == "amenity" &&
							(block.stringtable().s(v) == "pub" || block.stringtable().s(v) == "restaurant" || block.stringtable().s(v) == "bar")){
							pub = true;
						}
						else if (block.stringtable().s(k) == "name")
							name = &block.stringtable().s(v);
					}
					if (pub && name)
						std::cout << node.id() << " " << *name << "\n";
				}
				if (block.primitivegroup(g).has_dense()){
					const OSMPBF::DenseNodes &nodes = block.primitivegroup(g).dense();
					const std::string *name = 0;
					bool pub = false;

					int i = 0;
					int node = 0;
					uint64_t idBase = 0;
					while (i < nodes.keys_vals_size()){

						// new node
						if (nodes.keys_vals(i) == 0){
							idBase += nodes.id(node);
							i++;
							node++;
							pub = false;
							name = 0;
						} else {

							unsigned int k = nodes.keys_vals(i);
							unsigned int v = nodes.keys_vals(i+1);

							if (block.stringtable().s(k) == "amenity" &&
								block.stringtable().s(v) == "pub" || block.stringtable().s(v) == "restaurant" || block.stringtable().s(v) == "bar"){
								pub = true;
							} else if (block.stringtable().s(k) == "name"){
								name = &block.stringtable().s(v);
							}

							if (pub && name){
								std::cout << (idBase+nodes.id(node)) << " " << *name << "\n";
								while (nodes.keys_vals(i) != 0) i++;
								continue;
							}

							i += 2;
						}
					}
				}
				for (int n = 0; n < block.primitivegroup(g).relations_size(); n++){
					const OSMPBF::Relation &relation = block.primitivegroup(g).relations(n);
					const std::string *name = 0;
					bool pub = false;
					for (int key = 0; key < relation.keys_size(); key++){
						unsigned int k = relation.keys(key);
						unsigned int v = relation.vals(key);
						if (block.stringtable().s(k) == "amenity" &&
							(block.stringtable().s(v) == "pub" || block.stringtable().s(v) == "restaurant" || block.stringtable().s(v) == "bar")){
							pub = true;
						}
						else if (block.stringtable().s(k) == "name")
							name = &block.stringtable().s(v);
					}
					if (pub && name)
						std::cout << relation.id() << " " << *name << "\n";
				}
			}

		} else {
			std::cout << "Failed getting primitive block\n";
		}

	}

	return 0;
}
