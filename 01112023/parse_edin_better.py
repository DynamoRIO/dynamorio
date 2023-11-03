import re
import csv

# Read the input text file
with open('/home/edin/drio-data-collection/framework/dynamorio/01112023/1k_l1d_matmult_10.txt', 'r') as file:
    text = file.read()

# Define regular expressions for pattern matching
pattern = re.compile(r'\[(\d+)\](\w+)\s+(\d+)\s+byte\(s\)\s+@'
                     r'\s+0x([\da-fA-F]+)\s+(.*?)\n|(.*?) MISS\n|(.*?) MISS\n')

# Create a CSV file for writing
with open('output.csv', 'w', newline='') as csv_file:
    fieldnames = ['Line', 'Operation', 'ByteCount', 'Address', 'Instruction', "DATA_MISS", 'INST_MISS']
    writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
    writer.writeheader()

    lines = pattern.findall(text)
    data_miss = False
    inst_miss = False

    for i in range(len(lines)-1):
        line, operation, byte_count, address, instruction, miss_type,miss_type_2 = lines[i]
        line_next, operation_next, byte_count_next, address_next, instruction_next, miss_type_next,miss_type_2_next = lines[i+1]   
        
        data_miss = 1 if miss_type_next == "DATA" else 0
        inst_miss = 1 if miss_type_next == "INST" else 0
                 
        if miss_type == "":
            writer.writerow({'Line': line, 'Operation': operation, 'ByteCount': byte_count, 'Address': address, 'Instruction': instruction,
                            'DATA_MISS': data_miss, 'INST_MISS': inst_miss})
    
    # for line, operation, byte_count, address, instruction in lines:
    #     data_miss_flag = 1 if "DATA MISS" in line else 0
    #     inst_miss_flag = 1 if "INST MISS" in line else 0

    #     writer.writerow({'Line': line, 'Operation': operation, 'ByteCount': byte_count, 'Address': address, 'Instruction': instruction,
    #                      'DATA_MISS': data_miss_flag, 'INST_MISS': inst_miss_flag})

print("CSV file has been created.")
