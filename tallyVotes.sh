#!/bin/bash

# Check if the inputFile file exists and has appropriate permissions
if [ ! -f "inputFile.txt" ] || [ ! -r "inputFile.txt" ]; then
    echo "Error: inputFile does not exist or does not have appropriate permissions."
    exit 1
fi

# Sort and remove duplicate lines from the inputFile
sorted_input=$(sort "inputFile.txt" | uniq)

declare -A voter_counts
declare -A party_votes

while IFS=" " read -r name surname party; do
    # Skip empty lines
    if [ -z "$name" -a -z "$surname" -a -z "$party" ]; then
        continue
    fi
    
    voter="${name} ${surname}"
    
    # Check if the voter has already voted
    if [ -z "${voter_counts[$voter]}" ]; then
        voter_counts[$voter]=1
        
        # Count the vote for the party
        (( party_votes[$party]++ ))
    fi
    
done <<< "$sorted_input"

# Create tallyResultsFile and write the results
tally_results_file="$1"

# Clear the file if it already exists
> "$tally_results_file"

# Write the party-wise vote counts to the tallyResultsFile in sorted order
for party in $(echo "${!party_votes[@]}" | tr ' ' '\n' | sort); do
    vote_count="${party_votes[$party]}"
    echo "$party $vote_count" >> "$tally_results_file"
done

echo "Tally results have been written to $tally_results_file."
