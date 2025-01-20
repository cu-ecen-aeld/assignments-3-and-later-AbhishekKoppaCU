#!/bin/bash

# Check if exactly two arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Error: Two arguments required - <writefile> <writestr>"
    exit 1
fi

# Assign arguments to variables
writefile=$1
writestr=$2

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Write the string to the file, overwriting if it exists
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Could not create or write to the file '$writefile'"
    exit 1
fi

# Successful execution
exit 0
