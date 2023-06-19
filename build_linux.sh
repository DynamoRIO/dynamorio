# Install dependencies for Ubuntu 15+.  Adjust this command as appropriate for
# other distributions (in particular, use "cmake3" for Ubuntu Trusty).
sudo apt-get install cmake g++ g++-multilib doxygen git zlib1g-dev libunwind-dev libsnappy-dev liblz4-dev
# Make a separate build directory.  Building in the source directory is not
# supported.
mkdir build && cd build
# Generate makefiles with CMake.  Pass in the path to your source directory.
cmake ..
# Build.
make -j
# Run echo under DR to see if it works.  If you configured for a debug or 32-bit
# build, your path will be different.  For example, a 32-bit debug build would
# put drrun in ./bin32/ and need -debug flag to run debug build.
./bin64/drrun echo hello world