#!/bin/bash

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' 

BUILD_PROFILE="${1:-Release}"
echo -e "${BLUE}Build Profile: ${BUILD_PROFILE}${NC}"
echo ""

if [ ! -d "build" ]; then
    echo -e "${BLUE}Creating build directory...${NC}"
    mkdir build
fi

cd build

echo -e "${BLUE}Configuring CMake...${NC}"
cmake .. -DCMAKE_BUILD_PROFILE=${BUILD_PROFILE}

echo -e "${BLUE}Building project...${NC}"
cmake --build . --config ${BUILD_PROFILE} -- -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo -e "${GREEN}=== Build Complete! ===${NC}"
echo ""
echo -e "Run the application with:"
echo -e "  ${BLUE}./build/bin/Discove${NC}"
echo ""
