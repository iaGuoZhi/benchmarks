import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import sys

script = sys.argv[0]

if not len(sys.argv) == 2:
    sys.exit(f'Usage: python3 {script} work_dir option')
work_dir = sys.argv[1]

csv_filename = f'{work_dir}/disk_thr.csv'
png_filename = f'{work_dir}/disk_thr.png'
info_filename = f'{work_dir}/info.txt'

with open(info_filename, 'r') as file:
    info = file.read()

fig = plt.figure(figsize=(14, 5))
gs = gridspec.GridSpec(1, 3, width_ratios=[6,6,2])
ax1 = plt.subplot(gs[0])
ax2 = plt.subplot(gs[1])
ax3 = plt.subplot(gs[2])

df = pd.read_csv(csv_filename)
# First image
columns = df.columns[1:2]
for column in columns:
    ax1.plot(df['block_size'], df[column], label=column)

ax1.legend()
ax1.set_title(f'disk read throughput vs block size')
ax1.set_xlabel('Size')
ax1.set_ylabel('Bandwidth(mb/s)')
ax1.grid(True)

# Second image
columns = df.columns[2:]
for column in columns:
    ax2.plot(df['block_size'], df[column], label=column)

ax2.legend()
ax2.set_title(f'disk write throughput vs block size')
ax2.set_xlabel('Size')
ax2.set_ylabel('Bandwidth(mb/s)')
ax2.grid(True)

ax3.text(0.5, 0.5, info, fontsize=10, ha='center', va='center')
ax3.axis('off')

plt.savefig(png_filename)
