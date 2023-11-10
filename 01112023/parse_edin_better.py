import re
import csv


def parse_write_out_csv_file(path: str, cache_size_in_k: int, matrix_size: int) -> None:
    # Read the input text file
    with open(path, "r") as file:
        text = file.read()

    # Define regular expressions for pattern matching
    pattern = re.compile(
        r"\[(\d+)\](\w+)\s+(\d+)\s+byte\(s\)\s+@"
        r"\s+0x([\da-fA-F]+)\s+(.*?)\n|(.*?) MISS\n|(.*?) MISS\n"
    )

    # Create a CSV file for writing
    with open(
        f"./parsed_traces/{cache_size_in_k}k_l1d_matmult_{matrix_size}_parsed_total.csv",
        "w",
        newline="",
    ) as csv_file:
        fieldnames = [
            "Line",
            "Operation",
            "ByteCount",
            "Address",
            "Instruction",
            "CacheSize",
            "MatrixSize",
            "DATA_MISS",
            "INST_MISS",
        ]
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

        lines = pattern.findall(text)
        data_miss = False
        inst_miss = False

        for i in range(len(lines) - 1):
            (
                line,
                operation,
                byte_count,
                address,
                instruction,
                miss_type,
                miss_type_2,
            ) = lines[i]
            _, _, _, _, _, miss_type_next, _ = lines[i + 1]

            data_miss = 1 if miss_type_next == "DATA" else 0
            inst_miss = 1 if miss_type_next == "INST" else 0

            if miss_type == "":
                line = float(int(line)) / len(lines)
                writer.writerow(
                    {
                        "Line": line,
                        "Operation": operation,
                        "ByteCount": byte_count,
                        "Address": address,
                        "Instruction": instruction,
                        "CacheSize": cache_size_in_k,
                        "MatrixSize": matrix_size,
                        "DATA_MISS": data_miss,
                        "INST_MISS": inst_miss,
                    }
                )

    print(f"CSV file {csv_file.name} has been created.")


cache_sizes = [1, 2, 8, 128, 256, 512]
matrix_sizes = [10, 20, 50, 80, 100]


for cache in cache_sizes:
    for matrix_size in matrix_sizes:
        filename = f"./{cache}k_l1d_matmult_{matrix_size}.txt"
        parse_write_out_csv_file(filename, cache, matrix_size)
