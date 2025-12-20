#!/bin/bash
set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}Building in Debug mode...${NC}"
./build.sh Debug

if [ $? -ne 0 ]; then
    echo ""
    echo -e "${RED}Build failed! Cannot run application.${NC}"
    exit 1
fi

EXE_PATH="./build/bin/Discove"
if [ -f "$EXE_PATH" ]; then
    "$EXE_PATH"
    EXIT_CODE=$?

    echo ""
    if [ $EXIT_CODE -eq 0 ]; then
        echo -e "${GREEN}Application exited successfully.${NC}"
    else
        echo -e "${YELLOW}Application exited with code: $EXIT_CODE${NC}"
    fi
else
    echo ""
    echo -e "${RED}Executable not found at: $EXE_PATH${NC}"
    exit 1
fi
