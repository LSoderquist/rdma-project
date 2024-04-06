#!/bin/bash 

# Define the command line input
input="balance"
program="$2"

# Number of instances you want to run
num_invokes=$1

run() {
    # Loop to start multiple instances
    for ((i=0; i<$num_invokes; i++)); do
        # Run the a call to 'login'
        echo -e "${input} ${i}\nexit\n" | ./$program >/dev/null
    done
}

time run
