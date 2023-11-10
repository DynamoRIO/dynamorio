import csv

def remove_line_from_csv(input_file, output_file, line_to_remove):
    with open(input_file, 'r', newline='') as csvfile, open(output_file, 'w', newline='') as temp_output:
        csv_reader = csv.reader(csvfile)
        csv_writer = csv.writer(temp_output)
        
        for row in csv_reader:
            if row != line_to_remove:
                csv_writer.writerow(row)

    # Replace the original file with the temporary file
    import os
    os.replace(output_file, input_file)

# Usage
input_csv_file = 'merged-file.csv'
output_csv_file = 'merged-file.csv'
line_to_remove = ["percentage_execution","instruction_name","cache_miss_contribution","cache_size","matrix_size"]  # Replace with the actual line you want to remove

remove_line_from_csv(input_csv_file, output_csv_file, line_to_remove)
