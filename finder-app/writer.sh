#!/bin/bash

# only 2 arguments to be provided
if [ "$#" -ne 2 ]; then
    echo "Error: Two arguments required - <writefile> <writestr>"
    exit 1
fi

# Assign arguments to variables
writefile=$1
writestr=$2

# Create the directory path 
mkdir -p "$(dirname "$writefile")"

# Write the string to the file
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Could not create or write to the file '$writefile'"
    exit 1
fi

# On Successful execution
exit 0
