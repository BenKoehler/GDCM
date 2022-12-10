#!/bin/bash
cd ..
mv gdcm/ gdcm-src/
#remove previous installation
rm -r gdcm-build
rm -r gdcm
mkdir gdcm-build
mkdir gdcm
cd gdcm-build
cmake \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_INSTALL_PREFIX=../gdcm \
-DGDCM_BUILD_APPLICATIONS=Off \
-DGDCM_BUILD_EXAMPLES=Off \
-DGDCM_BUILD_SHARED_LIBS=On \
-DGDCM_TEMPORARY_DIRECTORY=../gdcm/temp \
-DSITE=bk \
../gdcm-src
make -j 8
make install
cd ..
rm -r gdcm-build
cp gdcm-src/bk.cmake gdcm

