# This script will plot temperature and trip graph from the thermald debug output
# Run thermald with loglevel=debug
# For example:
#
# systemctl stop thermald
# thermald --no-daemon --loglevel=debug --adaptive --ignore-cpuid-check
# Run your workload, and when done copy the output dumped on the screen
# to a file. For example if the output file is
# This output file is input to this script "thermald_log.txt"
# Then use:
# python thermald_debug_output_graph.py thermald_log.txt
#
# Prerequisites:
#    Python version 3.x or higher
#    gnuplot 5.0 or higher
#    python3-gnuplot 1.8 or higher
#    (Most of the distributions have these required packages. They may be called
#     gnuplot-py, python-gnuplot or python3-gnuplot, gnuplot-nox, ... )
#   In case Gnuplot is not present, the script will use pygnuplot. To do so, install
#   pygnuplot using pip install pygnuplot. In case can't install with pip, download
#   the pygnuplot.py file from https://pypi.org/project/py-gnuplot
#   To install use "python3 setup.py install". It may ask for pandas, install it
#   using "pip install pandas" or from distro install python3-pandas.

import sys
import re

use_pygnuplot = False

try:
    import Gnuplot
except ImportError:
    from pygnuplot import gnuplot
    use_pygnuplot = True

sensors = []
temperature = [[]]
temperature_count = []
max_range = 0

zones = []
trips = [[]]

# Parse file for temperature


def plot_temperature():
    output_png = "temperature.png"
    if use_pygnuplot:
        g_plot = gnuplot.Gnuplot()
    else:
        g_plot = Gnuplot.Gnuplot()

    g_plot('set ylabel "Degree C"')
    g_plot('set xlabel "Samples"')
    g_plot("set yrange [0:100]")
    g_plot("set key autotitle columnhead")
    g_plot('set title "Temperature plot"')
    g_plot("set term png size 1200, 600")
    g_plot('set output "' + output_png + '"')
    print("Ploting using gnuplot, the output file name is : ", output_png)
#    g_plot('plot "' + output_png + '" using 1:(column(n)) w lines title columnhead(n), for [n=2:50] "trips.csv" using 1:(column(n)) w dots title # columnhead(n)')
    g_plot(
        'plot for [n=2:12] "temperature.csv" u 1:(column(n)) w lines title columnhead(n), for [n=2:50] "trips.csv" u 1:(column(n)) w dots title columnhead(n)'
    )


def parse_trips(log_file):
    print("parsing trips from", log_file)
    log_fd = open(log_file, "r")
    data = log_fd.readlines()
    match_index = 0
    line_index = 0
    for line in data:
        x = re.search("ZONE DUMP BEGIN", line)
        if x:
            match_index = line_index
        line_index = line_index + 1

    for x in range(match_index, line_index):
        s = re.search("Zone.*:", data[x])
        if s:
            tokens = data[x].split()
            zone = tokens[2].split(",")
            zones.append(zone[0])

        y = re.search("ZONE DUMP END", data[x])
        if y:
            break

    index = 0
    trips.append([])
    # 3 for point to 3rd line starting from "ZONE DUMP BEGIN"
    for x in range(match_index + 3, line_index):
        s = re.search("passive temp:*", data[x])
        if s:
            tokens = data[x].split()
            trip = tokens[3].split(":")
            str1 = str(int(trip[1]) / 1000)
            trips[index].append(str1)

        z = re.search("Zone.*:", data[x])
        if z:
            index = index + 1
            trips.append([])

        y = re.search("ZONE DUMP END", data[x])
        if y:
            break

    max_index = index

    f_handle = open("trips.csv", "w")

    string_buffer = "index, "

    for x in range(len(zones)):
        id = 0
        for y in range(len(trips[x])):
            string_buffer += zones[x]
            string_buffer += "_trip_0"
            string_buffer += str(id)
            if x != (len(zones) - 1) and y != (len(trips) - 1):
                string_buffer += ", "
            id = id + 1

    string_buffer += "\n"
    f_handle.write(string_buffer)

    string_buffer = "0,"

    for i in range(max_range):
        string_buffer = str(i)
        string_buffer += ", "
        for x in range(len(zones)):
            for y in range(len(trips[x])):
                string_buffer += trips[x][y]
                if x != (len(zones) - 1) and y != (len(trips) - 1):
                    string_buffer += ", "

        string_buffer += "\n"
        f_handle.write(string_buffer)

    f_handle.close()

    f_handle.close()


def parse_temperature(log_file):
    # Parse file for temperature
    global max_range

    print("parsing temperature from", log_file)

    sensors.append("PL1")
    temperature_count.append(0)
    temp = []
    temperature.append(temp)

    log_fd = open(log_file, "r")
    data = log_fd.readlines()
    next_line = 0
    for line in data:
        next_line = next_line + 1
        x = re.search(".*wr:", line)
        if x:
            tokens = line.split()
            tokens = tokens[7].split(":")
            str1 = tokens[1]
            val = int(str1)
            val = val / 1000000
            if (val > 100):
                continue;
            str1 = str(val)
            temperature[0].append(str1)
            temperature_count[0] = temperature_count[0] + 1
            if max_range < temperature_count[0]:
                max_range = temperature_count[0]

        x = re.search("Sensor.*:temp", line)
        if not x:
            x = re.search("Sensor.*:power", line)

        if x:
            if "compare" in data[next_line]:
                continue
            tokens = line.split()
            skip = 0
            i = 0
            for sensor in sensors:
                if sensor == tokens[1]:
                    skip = 1
                    break
                i = i + 1
            if skip == 0:
                sensors.append(tokens[1])
                temperature_count.append(0)
                temp = []
                temperature.append(temp)

            str1 = str(int(tokens[3]) / 1000)

            temperature[i].append(str1)

            temperature_count[i] = temperature_count[i] + 1
            if max_range < temperature_count[i]:
                max_range = temperature_count[i]

    f_handle = open("temperature.csv", "w")

    last_val= []
    string_buffer = "index, "
    for x in range(len(sensors)):
        last_val.append(0)
        if x == len(sensors) - 1:
            string_buffer += sensors[x]
        else:
            string_buffer += sensors[x] + ", "

    string_buffer += "\n"
    f_handle.write(string_buffer)

    index = 0
    for y in range(max_range):
        string_buffer = str(index) + ", "
        index = index + 1
        for x in range(len(sensors)):
            if y < temperature_count[x]:
                last_val[x] = temperature[x][y]
                if x == len(sensors) - 1:
                    string_buffer += temperature[x][y]
                else:
                    string_buffer += temperature[x][y] + ", "

            else:
                string_buffer += last_val[x] + ", "

        string_buffer += "\n"
        f_handle.write(string_buffer)

    f_handle.close()

    log_fd.close()


# Main entry point
# Get the debug file name from command line
print("Number of arguments:", len(sys.argv), "arguments.")
print("Argument List:", str(sys.argv))

if len(sys.argv) < 2:
    print("Error: Need the logfile as argument")
    sys.exit()

# Parse temperature
parse_temperature(str(sys.argv[1]))
parse_trips(str(sys.argv[1]))
plot_temperature()
