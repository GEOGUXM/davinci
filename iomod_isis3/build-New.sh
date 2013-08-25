#! /bin/sh
echo Compiling ISIS3 module....
ISIS3DIR=/mars/common/isis3/x86_64_Linux/isis
QTSRCDIR=/usr
ARCH=64
LD_RUN_PATH=$ISIS3DIR/3rdParty/lib
echo $LD_RUN_PATH
g++ -DHAVE_CONFIG_H -m$ARCH -fPIC -g -I.. -I../libltdl -I$ISIS3DIR/inc -I$QTSRCDIR/include -I/$QTSRCDIR/include/QtCore -Wall iomod_isis3.C -c -o iomod_isis3.o
if [ $? != 0 ]; then
    echo Compile failed.
    exit 1
fi
echo Linking ISIS3 module...
g++ -m$ARCH -fPIC -shared -Wl,-soname,isis3.dvio -Wl,-rpath=$ISIS3DIR/3rdParty/lib isis3.dvio iomod_isis3.o -o isis3.dvio -L$ISIS3DIR/lib -L$ISIS3DIR/3rdParty/lib -lisis3 -lkdu_a63R -lgsl -lgeos_c -lqwt -lxerces-c -lprotobuf
if [ $? != 0 ]; then
    echo Link failed.
    exit 1
fi
echo "Build of ISIS3 I/O module successful."