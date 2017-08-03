#--------------------------------------------------------------------
# This is a rather ugly python script for plotting experimental results
# generated by the output of generate_experiment_script.sh. It contains logic
# for parsing an output directory tree in ./out that looks like this:

# out
# |-- ATT_encrypted_results (not important / necessary, contents can be directly under out)
# |   |-- quality_50_bits_0_block_16 (this is the experiment name)
# |   |   |-- 1
# |   |   |   |-- test.log
# |   |   |   `-- train.log
# |   |   |-- 2
# |   |   |   |-- test.log
# |   |   |   `-- train.log
# |   |   |-- 3
# |   |   |   |-- test.log
# |   |   |   `-- train.log
# |   |-- quality_50_bits_0_block_24
# |   |   |-- 1
# |   |   |   |-- test.log
# |   |   |   `-- train.log
# |   |   |-- 2
# |   |   |   |-- test.log
# |   |   |   `-- train.log
# ...
#
# At the bottom level are test.log/train.log, which are generated by
# figleaf-nn-train.lua. These are the results of the neural network testing and
# training at each epoch. This data is loaded in and parsed.
#
# NOTE: This script has a few limitations. It expects that experiments are
# four-dimensional, and that the name of the experiment folder contains three
# integers, under which are folders named with a single integer. Probably the
# single biggest TODO for this tool is to remove this arbitrary requirement,
# which would make it more flexible.

# This script written by Byron Marohn for a 2017 independent study with Dr.
# Charles Wright at Portland State University.
#
#--------------------------------------------------------------------

import numpy as np
import re
import os
from operator import itemgetter
import matplotlib.pyplot as plt

# Always print full numpy arrays
np.set_printoptions(threshold=np.nan)

EXPECTED_NUM_FLOATS=150
parameter_names = ["JPEG Quality", "Sacrificial Thumbnail Bits", "Block Size", "Test/Train Split Index"]

def column(matrix, i):
    return [row[i] for row in matrix]

# Given a filepath, load floating point data into a list and return it. Note
# the first line is removed (assumed to be a header)
def load_file_floats(filepath):
    # Open file for reading
    f = open(filepath, "r")
    lines = f.readlines()
    f.close()

    # Remove first line header
    lines = lines[1:]

    # Convert loaded file to floats
    floats = np.array(lines).astype(np.float)

    # Print error if the file doesn't contain the right number of entries
    if (len(floats) != EXPECTED_NUM_FLOATS):
        print("Error: Expected " + str(EXPECTED_NUM_FLOATS) + " floats, got " + str(len(floats)))
        exit(1)

    return floats

# Given an experiment directory name, parse out a list of the integer values in that name and return them.
# For example, given quality_50_bits_0_block_16, the list returned will be [50, 0, 16]
def parse_dir_name_numbers(dirname):
    first_num_regex = re.compile("^[^0-9]*([0-9]*)[^0-9]*.*$")
    del_to_first_num_regex = re.compile("^[^0-9]*[0-9]*([^0-9]*.*)$")

    # Iterate over the directory and parse it into a list of numbers
    values = []
    while dirname != "":
        values.append(int(first_num_regex.sub('\g<1>',dirname)))
        dirname = del_to_first_num_regex.sub('\g<1>',dirname)

    # Print error if unexpected number of parameters
    if (len(values) != len(parameter_names)):
        print("Error: Expected " + str(len(parameter_names)) + " values, got " + str(len(values)))
        exit(1)

    return values

# Read all the input data into a single list of lists
alldata = []
for path, dirs, files in os.walk("./out"):
    if "test.log" in files and "train.log" in files:
        # Load test and train data (float arrays)
        testfloats = load_file_floats(path + "/test.log")
        trainfloats = load_file_floats(path + "/train.log")

        # Parse the folder name to see what parameters that data has
        testparams = parse_dir_name_numbers(path)

        # Combine this entire dataset into an array and append to the master copy
        testparams.append(trainfloats)
        testparams.append(testfloats)
        alldata.append(testparams)
#print alldata

# TODO: For the rest of this script, there's a bunch of hardcoded indexes,
# which effectively requires the directory structure explained in the block
# describing how this script works. It should be made generic so that
# experiments can have a number of dimensions/arguments other than 4

# Pick out just the last train/test data point and create a new list of lists
last_epoch_data = []
for entry in alldata:
    last_epoch_data.append([entry[0], entry[1], entry[2], entry[3], entry[4][-1], entry[5][-1]])
#print last_epoch_data

# Sort that list of lists by the number of parameters
last_epoch_data.sort(key=itemgetter(*range(0,len(parameter_names))))
#print(np.array(last_epoch_data))

# Compute mean and standard deviation for each test set
condensed_data = []
last_params=""
for entry in last_epoch_data:
    this_params = ''.join(str(entry[0:len(parameter_names)-1]))
    if (this_params != last_params):
        if (last_params != ""):
            condensed_data.append(["Quality:" + str(lastentry[0]) + "  Bits:" + str(lastentry[1]) + "  Block Size:" + str(lastentry[2]), lastentry[0], lastentry[1], lastentry[2], np.mean(trainarray), np.std(trainarray), np.mean(testarray), np.std(testarray)])
        trainarray = np.array([])
        testarray = np.array([])
        last_params = this_params

    trainarray = np.append(trainarray, entry[len(parameter_names)])
    testarray = np.append(testarray, entry[len(parameter_names) + 1])
    lastentry = entry
condensed_data.append(["Quality:" + str(lastentry[0]) + "  Bits:" + str(lastentry[1]) + "  Block Size:" + str(lastentry[2]), lastentry[0], lastentry[1], lastentry[2], np.mean(trainarray), np.std(trainarray), np.mean(testarray), np.std(testarray)])
#print np.array(condensed_data)

# 0 - title, 1 - quality, 2 - bits, 3 - block, 4 - train, 5 - train std dev, 6 - test, 7 - train_std_dev

# Sort list of lists by block size, bits, jpeg quality
condensed_data.sort(key=itemgetter(2,3,1))
condensed_data.append(["Random Guessing (1 in 40 chance)", 0, 0, 0, 100.0/40.0, 0, 100.0/40.0, 0])
#print(np.array(condensed_data))


plt.rcdefaults()
fig, ax = plt.subplots()

headers = column(condensed_data, 0)
y_pos = np.arange(len(headers))
plotdata = np.array(column(condensed_data, 6))
stddev = np.array(column(condensed_data, 7))

ax.barh(y_pos, plotdata, xerr=stddev, align='center',
        color='green', ecolor='black')
ax.set_yticks(y_pos)
ax.set_yticklabels(headers)
ax.invert_yaxis()  # labels read top-to-bottom
ax.set_xlabel('Average % Correct (Test Data)')
ax.set_title('McPherson et. al. ATT Neural Network vs JPEG TPE by LSB Embedding')

plt.show()
# Compute average and standard deviation for
#for entry in last_epoch_data

