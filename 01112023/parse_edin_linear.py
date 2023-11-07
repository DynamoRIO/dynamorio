import re
import math

#### TODO: this hasn't been modified yet. Must fix stuff before the linear idea can be tested. But certainly, there is possibility there.

def parse_file(filename, cache_size, matrix_size):
    parsed_filename = "./parsed_traces/"+filename.strip(".txt") + "_parsed_binary_class.csv"
    writeline = "\[.*\]write(.*)"
    readline = "\[.*\]read(.*)"
    ifetch = "\[(\d+)\]ifetch.*@(.*)"

    last_miss = -1

    output = []
    total_lines = 0

    for line in reversed(list(open(filename))):
        l = line.strip()
        # print(">", l)
        if total_lines == 0 and re.match("\[(\d+)\]", l):
            r = re.findall("\[(\d+)\]", l)
            total_lines = int(r[0])
        if l == "":
            ()
        elif "DATA MISS" in l:
            last_miss = 0
            # print("\t data miss")
        elif "INST MISS" in l:
            # print("\t inst miss")
            ()
        elif re.match(readline, l):
            # print("\tread")
            ()
        elif re.match(writeline, l):
            # print("\twrite")
            ()
        elif re.match(ifetch, l):
            f = re.findall(ifetch, l)
            inst = f[0][1][41:].split(" ")[0]
            # print("\tifetch", inst)
            i = int(f[0][0])
            if last_miss >= 0:
                if last_miss == 0:
                    output.insert(0, [i/total_lines, inst, 1])
                else:
                    output.insert(0, [i/total_lines, inst, 0])
                last_miss += 1
        elif l == "00" or l == "00 00 00" or l == "00 00 00 00":
            ()
        elif "entry type" in l:
            ()
        else:
            # print("Unknown> ", l)
            # todo: make this better, but could be uninmportant!
            # exit()
            ()

    print("End of file!")


    fout = open(parsed_filename, "w+")
    fout.write("percentage_execution,instruction_name,cache_miss_contribution,cache_size,matrix_size"+ "\n")
    for o in output:
        fout.write(str(o[0]) + "," + str(o[1]) + "," + str(o[2]) + "," + cache_size.trim("k") + ","+matrix_size + "\n")
    fout.close
    
def parse_file_return_output(filename, cache_size, matrix_size, output):
    parsed_filename = "./parsed_traces/"+filename.strip(".txt") + "_parsed.csv"
    writeline = "\[.*\]write(.*)"
    readline = "\[.*\]read(.*)"
    ifetch = "\[(\d+)\]ifetch.*@(.*)"

    last_miss = -1

    total_lines = 0

    for line in reversed(list(open(filename))):
        l = line.strip()
        # print(">", l)
        if total_lines == 0 and re.match("\[(\d+)\]", l):
            r = re.findall("\[(\d+)\]", l)
            total_lines = int(r[0])
        if l == "":
            ()
        elif "DATA MISS" in l:
            last_miss = 0
            # print("\t data miss")
        elif "INST MISS" in l:
            # print("\t inst miss")
            ()
        elif re.match(readline, l):
            # print("\tread")
            ()
        elif re.match(writeline, l):
            # print("\twrite")
            ()
        elif re.match(ifetch, l):
            f = re.findall(ifetch, l)
            inst = f[0][1][41:].split(" ")[0]
            # print("\tifetch", inst)
            i = int(f[0][0])
            if last_miss >= 0:
                if last_miss == 0:
                    output.insert(0, [i/total_lines, inst,1, cache_size.strip('k'), matrix_size])
                else:
                    output.insert(0, [i/total_lines, inst, 0, cache_size.strip('k'), matrix_size])
                last_miss += 1
        elif l == "00" or l == "00 00 00" or l == "00 00 00 00":
            ()
        elif "entry type" in l:
            ()
        else:
            # print("Unknown> ", l)
            # todo: make this better, but could be uninmportant!
            # exit()
            ()

    return output





cache_sizes = ["1k","2k","8k","128k","256k","512k"]
matrix_sizes = ["10","20","50","80","100"]

total_count = len(cache_sizes)*len(matrix_sizes)
counter = 0
output = []
for cache in cache_sizes:
    for matrix_size in matrix_sizes:
        counter += 1
        filename = f"{cache}_l1d_matmult_{matrix_size}.txt"
        output += parse_file_return_output(filename, cache, matrix_size, output)
        print(f"Done {counter}/{total_count}, {float(counter)/total_count*100.0}%")

fout = open("./parsed_traces/full_execution_sigmoid_megafile.csv", "w+")
fout.write("percentage_execution,instruction_name,cache_miss,cache_size,matrix_size"+ "\n")
for o in output:
    fout.write(str(o[0]) + "," + str(o[1]) + "," + str(o[2]) + "," + str(o[3]) + "," + str(o[4])+ "\n")
fout.close