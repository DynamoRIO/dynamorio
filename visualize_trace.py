#!/usr/bin/python3
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

assert len(sys.argv) > 1, "provide a trace csv file as argument!"

# Load the data from the CSV file
data = pd.read_csv(sys.argv[1], skiprows=[0]) # because "---- <application exited with code 0> ----"
# TODO should the address space be bisected? seems like it's bimodal and the distance between peaks is so big that it squishes all the points to the edges

# Extract the 'x', 'y', and 'z' columns from the DataFrame
x = data['timestamp']
y_hex = data['address']
y = [int(address, 16) for address in y_hex]
z = data['count']


# Create a 3D plot
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

# Create a wireframe 3D plot
ax.scatter(x, y, z, c='b', marker='o')

# Set labels for the axes
ax.set_xlabel('timestamp')
ax.set_ylabel('address')
ax.set_zlabel('frequency')
ax.set_title('Stores and Loads')


# Annotate data points with hexadecimal addresses

# need to do some quantization here lol this generates a billion glyphs
#for i, txt in enumerate(y_hex):
    #ax.text(x[i], y[i], z[i], txt, fontsize=8)

# Display the plot
#plt.show()
plt.savefig("tmp.png")
