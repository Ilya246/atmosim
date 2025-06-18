#!/bin/bash

NAME="radius-13.3r-21.5s-PT+O"

# Execute the simulation command and capture both stdout and stderr
output=$(./atmosim --simpleout --ticks 120 -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 --silent 2>&1)
exit_status=$?

# Check if the command executed successfully
if [ $exit_status -ne 0 ]; then
    echo "Error: Command failed with exit status $exit_status" >&2
    echo "Output: $output" >&2
    exit $exit_status
fi

# Extract the first number after 'os=' from the output
if [[ ! "$output" =~ os=([0-9.]+) ]]; then
    echo "Error: Failed to extract optstat value from output" >&2
    echo "Output: $output" >&2
    exit 1
fi

os_value="${BASH_REMATCH[1]}"

# Compare the extracted value with 13 using bc for floating-point arithmetic
if (( $(echo "$os_value > 13" | bc -l) )); then
    echo "Test success: found $NAME with radius $os_value"
    exit 0
else
    echo "Test failed: found <13 radius while attempting to find $NAME"
    exit 1
fi
