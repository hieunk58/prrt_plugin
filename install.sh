DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/PRRT/build
cmake .. && make clean && make -j9
cd $DIR/plugin
./autogen.sh && make clean && make -j9
