# Setup
```bash
mkdir build
cd build
cmake --configure ..
cmake .
make
bin64/drrun -t drcachesim  -simulator_type elam -- ls
```

# Usage
`make && bin64/drrun -t drcachesim  -simulator_type elam -- ls 2> data.csv`
there's probably a good way to get results to a file that's robust to programs polluting stderr but i do not care for now
`python3 visualize_trace.py data.csv`
