import numpy as np
import pandas as pd
from typing import List

def interpolate_linear_between_consec_indices(dataframe:pd.DataFrame,index_list:List[int], column_name:str) -> None:
    for i in range(len(index_list) - 2):
        present_index = index_list[i]
        future_index = index_list[i+1]
        # print(f"present: {present_index}, future: {future_index}")
        k = 1.0/float(future_index-present_index)
        b = present_index/float(present_index-future_index)
        for j in range(present_index,future_index):
            dataframe.at[j, column_name] = round(k*float(j)+b, 3)

def binary_csv_to_linear(path: str, cache_size_in_k: int, matrix_size: int) -> None:

    # Load the CSV file into a DataFrame
    df = pd.read_csv(path)
    
    data_miss_indices = df[df["DATA_MISS"] == 1].index.tolist()
    instruction_miss_indices = df[df["INST_MISS"] == 1].index.tolist()
    # data_miss_indices.insert(0,0)
    # instruction_miss_indices.insert(0,0)
    
    interpolate_linear_between_consec_indices(df, data_miss_indices, "DATA_MISS")
    interpolate_linear_between_consec_indices(df, instruction_miss_indices, "INST_MISS")
    
    for idx in data_miss_indices:
        df.at[idx, "DATA_MISS"] = 1
    for idx in instruction_miss_indices:
        df.at[idx, "INST_MISS"] = 1
    
    # Save CSV to file
    filename = f"{cache_size_in_k}k_l1d_matmult_{matrix_size}_parsed_total_linear.csv"
    df.to_csv("./parsed_traces/"+filename)
    
    print(f"CSV file {filename} has been created.")


cache_sizes = [1, 2, 8, 128, 256, 512]
matrix_sizes = [10, 20, 50, 80, 100]


for cache in cache_sizes:
    for matrix_size in matrix_sizes:
        filename = f"./parsed_traces/{cache}k_l1d_matmult_{matrix_size}_parsed_total.csv"
        binary_csv_to_linear(filename, cache, matrix_size)
