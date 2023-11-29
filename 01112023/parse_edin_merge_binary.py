import re
import csv
import pandas as pd
from typing import List


def merge_binary_csvs(
    path_to_csvs_folder: str,
    cache_sizes_in_k: List[int],
    matrix_sizes: List[int],
    percentage_list: List[int],
) -> pd.DataFrame:
    # Create empty dataframe
    df_total = pd.DataFrame()

    for cache_size in cache_sizes_in_k:
        for matrix_size in matrix_sizes:
            path_to_csv = f"{path_to_csvs_folder}/{cache_size}k_l1d_matmult_{matrix_size}_parsed_total.csv"
            df_temp = pd.read_csv(path_to_csv)
            df_indices = [
                percentage / 100.0 * len(df_temp) for percentage in percentage_list
            ]
            if df_indices[0] != 0:  # add zero just in case it has been forgotten
                df_indices.insert(0, 0)
            if df_indices[-1] != 100:  # add hundred just in case it has been forgotten
                df_indices.insert(-1, len(df_temp))

            for i in range(0, len(df_indices) - 1, 2):
                sub_df = df_temp.loc[df_indices[i] : df_indices[i + 1]]
                df_total = pd.concat([df_total, sub_df], ignore_index=True)
            
            print(f"Finished with the CSV with cache {cache_size}k and matrix size {matrix_size}")

    # Save dataframe as CSV
    df_total.to_csv(f"{path_to_csvs_folder}/merged_binary_all.csv", index=False)

    # Return it if it is needed later
    return df_total


cache_sizes = [1, 2, 8, 128, 256, 512]
matrix_sizes = [10, 20, 50, 80, 100]

csv_folder_path = "./parsed_traces"

merge_binary_csvs(
    csv_folder_path, cache_sizes, matrix_sizes, [0, 10, 30, 40, 60, 70, 90, 100]
)
