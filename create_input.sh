#!/bin/bash

# Check the number of arguments
if [ "$#" -ne 2 ]; then
    echo "Error: Incorrect number of arguments."
    echo "Usage: $0 politicalParties.txt numLines"
    exit 1
fi

# Assign input arguments to variables
parties_file="$1"
num_lines="$2"

# Check if the parties file exists
if [ ! -f "$parties_file" ] || [ ! -r "$parties_file" ]; then
    echo "Error: File $parties_file not found or does not have appropriate permissions."
    exit 1
fi

# Check if numLines is a positive integer
if [ "$num_lines" -le 0 ]; then
    echo "Error: numLines should be a positive integer."
    exit 1
fi

# Count the number of parties in the file
num_parties=$(wc -l < "$parties_file")

# Check if the parties file is empty
if [ "$num_parties" -eq 0 ]; then
    echo "Error: The parties file is empty."
    exit 1
fi

input_file="inputFile.txt"

# Clear the file if it already exists
> "$input_file"

# Generate numLines lines in the input file
for ((i=0; i<num_lines; i++)); do
    # Generate random first name
    first_name_length=$(( RANDOM % 10 + 3 ))
    first_name=$(tr -dc '[:alpha:]' < /dev/urandom | head -c "$first_name_length")

    # Generate random last name
    last_name_length=$(( RANDOM % 10 + 3 ))
    last_name=$(tr -dc '[:alpha:]' < /dev/urandom | head -c "$last_name_length")

    # Select random party from the parties file
    party_number=$(( RANDOM % num_parties + 1 ))
    party=$(sed -n "${party_number}p" "$parties_file")

    # Append generated line to the input file
    echo "$first_name $last_name $party" >> "$input_file"
done
echo "Input file created: $input_file"


