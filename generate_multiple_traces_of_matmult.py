import os


cache_sizes = ["1k","2k","8k","128k","256k","512k"]
matrix_sizes = ["10","20","50","80","100"]

for cache in cache_sizes:
    for matrix_size in matrix_sizes:
        os.system(f"mkdir traces/01112023/{cache}_l1d_matmult_{matrix_size}.dir")
        os.system(f"./build/bin64/drrun -t drcachesim -L1D_size {cache} -offline -outdir traces/01112023/{cache}_l1d_matmult_{matrix_size}.dir -- ./../application/matmult {matrix_size}")
for cache in cache_sizes:
    for matrix_size in matrix_sizes:
        os.system(f"./build/bin64/drrun -t drcachesim -simulator_type missing_instructions -indir traces/01112023/{cache}_l1d_matmult_{matrix_size}.dir/* 2>&1 | tee ./01112023/{cache}_l1d_matmult_{matrix_size}.txt")

