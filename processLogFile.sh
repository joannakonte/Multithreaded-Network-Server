#!/bin/bash

# Check if the inputFile file exists and has appropriate permissions
if [ ! -f "$1" ] || [ ! -r "$1" ]; then
    echo "Error: $1 does not exist or does not have appropriate permissions."
    exit 1
fi

# Sort and remove duplicate lines from the inputFile
sorted_input=$(sort "$1" | uniq)

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

# Create pollerResultsFile and write the results
poller_results_file="pollerResultsFile"

# Clear the file if it already exists
> "$poller_results_file"

# Write the party-wise vote counts to the pollerResultsFile 
for party in $(echo "${!party_votes[@]}" | tr ' ' '\n' | sort); do
  vote_count="${party_votes[$party]}"
  echo "$party $vote_count" >> "$poller_results_file"
done

echo "Results have been written to $poller_results_file."
