#!/bin/bash

# Function to clear the terminal screen
clear_screen() {
    clear
}

# Function to monitor file changes
monitor_file() {
    # Initial content of the file
    initial_content=$(cat "$1")

    while true; do
        # Current content of the file
        current_content=$(cat "$1")

        # Check for changes in content
        if [ "$initial_content" != "$current_content" ]; then
            clear_screen
            echo "File content changed:"
            cat "$1" # Display the new content
            initial_content="$current_content"
        fi

        # Wait for a short duration before checking again
        sleep 1
    done
}

# Check if the file path is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <file_path>"
    exit 1
fi

# Start monitoring the file
monitor_file "$1"
