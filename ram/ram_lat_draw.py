import csv
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import math
import sys

script = sys.argv[0]

if not len(sys.argv) == 2:
    sys.exit(f'Usage: python3 {script} work_dir')
work_dir = sys.argv[1]

csv_filename = f'{work_dir}/ram_lat.csv'
png_filename = f'{work_dir}/ram_lat.png'
info_filename = f'{work_dir}/info.txt'

data=[]
with open(csv_filename, 'r') as file:
    reader = csv.reader(file)
    for row in reader:
        data.append(int(row[1]))

# Compute average,median,minimal,maximum values
avg_lat = round(sum(data)/len(data),2)
med_lat = sorted(data[:])[len(data) // 2]
min_lat = sorted(data[:])[0]
max_lat = sorted(data[:])[-1]
print(f'Average latency(ns): {avg_lat}\nMedian latency(ns): {med_lat}\nMinimal latency(ns): {min_lat}\nMaximum latency(ns): {max_lat}')

with open(info_filename, 'r') as file:
    info = file.read()
info += (f'Average latency(ns): {avg_lat}\n')
info += (f'Median latency(ns): {med_lat}\n')
info += (f'Minimal latency(ns): {min_lat}\n')
info += (f'Maximum latency(ns): {max_lat}\n')

fig = plt.figure(figsize=(14, 5))
gs = gridspec.GridSpec(1, 3, width_ratios=[6,6,2])
ax1 = plt.subplot(gs[0])
ax2 = plt.subplot(gs[1])
ax3 = plt.subplot(gs[2])

# First image
counts, bin_edges, patches = ax1.hist(data, bins=1200, density=True, facecolor='g', alpha=0.75)

ax1.set_title('Fig1 Latency distribution (x log scale)')
ax1.set_xlabel('Latency(ns)')
ax1.set_ylabel('Probability')
ax1.set_xscale('log')
ax1.grid(True)


# Second image
ax2.hist(data, bins=1200, density=True, facecolor='g', alpha=0.75)
ax2.set_title('Fig2 Latency distribution (x and y log scale)')
ax2.set_xlabel('Latency(ns)')
ax2.set_ylabel('Probability')
ax2.grid(True)
ax2.set_xscale('log')
ax2.set_yscale('log')

# Annotate the plot with the bin_percentages
num_bins = len(bin_edges) - 1
bin_percentages = counts / np.sum(counts) * 100
info += ('\nSpikes:\n')
for i in range(num_bins):
    y = bin_percentages[i]
    if y >= 0.5:
        y = round(y, 2)
        print('Range %d - %d frequency: %.2f%%' % (bin_edges[i], bin_edges[i + 1], y))
        info += ('Range %d - %d: %.2f%%\n' % (bin_edges[i], bin_edges[i + 1], y))
ax3.text(0.5, 0.5, info, fontsize=10, ha='center', va='center')
ax3.axis('off')

plt.savefig(png_filename)
plt.show()
