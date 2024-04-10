import os
import time

def file_modified_within_time(filepath, minutes):
    if not os.path.exists(filepath):
        print("File does not exist.")
        return False
    
    current_time = time.time()
    modification_time = os.path.getmtime(filepath)
    time_difference = current_time - modification_time
    
    minutes_difference = time_difference / 60  # Convert seconds to minutes
    print(minutes_difference,time_difference)
    if minutes_difference < minutes:
        return True
    else:
        return False

# Example usage:
filepath = "test_cache.c"  # Replace with your file path
if file_modified_within_time(filepath, 10):
    print("File was modified within the last 10 minutes.")
else:
    print("File was not modified within the last 10 minutes.")
