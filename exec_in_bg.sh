#!/bin/bash 

# Define the command line input
input="balance 1\nexit\n"
program="$2"

# Number of instances you want to run
num_instances=$1

# Function to continuously feed input to the program
run_instance() {
    while true; do
        # Feed the input to the program
        echo -e "$input" | ./$program >/dev/null 2>&1 &
        #sleep 1  # Adjust the sleep duration as needed
    done 
}

# Loop to start multiple instances
for ((i=1; i<=$num_instances; i++)); do
    # Run the function in the background
    run_instance &
done
