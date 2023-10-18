# TransportCatalogue

## *Educational project*

- Accepts JSON format data as input and produces a response in the form of an SVG file that visualizes stops and routes.
- Finds the shortest route between stops.
- To speed up calculations, the reference database has been serialized via Google Protobuf.
- Implemented a JSON constructor that allows you to find an incorrect sequence of methods at the compilation stage.
---
## Instructions for assembling the project (Visual Studio)
1. Install Google Protobuf. Download the protobuf-cpp archive from the [official repository](https://github.com/protocolbuffers/protobuf/releases) and unpack it on your computer.
2. Create folders build-debug and build-release to build two Protobuf configurations. First create a package folder in which the Protobuf package will be stored.
3. Build and install the project (in the example Debug build) using the following commands:
```
cmake path\protobuf-3.15.8\cmake -DCMAKE_SYSTEM_VERSION=10.0.17763.0 -DCMAKE_BUILD_TYPE=Debug \ -Dprotobuf_BUILD_TESTS=OFF
\ -DCMAKE_INSTALL_PREFIX=path\protobuf-3.15.8\package
cmake --build .
cmake --install .
```
4. Bin\protoc.exe has appeared in the package folder - it will be used to copy proto files, and in the lib folder - static libraries for working with Protobuf.
5. To compile the proto file, you need to run the following command:
`<protobuf package path>\bin\proto --cpp_out . transport_catalogue.proto`
6. Build the project using CMake:
```
cmake . -DCMAKE_PREFIX_PATH=/path/to/protobuf/package
cmake --build .
```
7. If necessary, add the include and lib folders to the additional project dependencies - Additional Include Directories and Additional Dependencies.
---
## Running the program
To create a transport directory database and serialize it into a file using base_requests, you need to run the program with the make_base parameter, specifying the input JSON file.
An example of running a program to populate a database:
`transport_catalogue.exe make_base <base.json`

In order to use the resulting database and deserialize it to respond to stat_requests requests, you need to run the program with the process_requests parameter, specifying the input JSON file containing requests to the database and the output file that will contain responses to requests.
An example of running a program to perform database queries:
`transport_catalogue.exe process_requests <req.json >out.txt`

---
