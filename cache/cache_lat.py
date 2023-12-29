import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import sys

script = sys.argv[0]

if not len(sys.argv) == 3:
    sys.exit(f'Usage: python3 {script} work_dir option')
work_dir = sys.argv[1]
option = sys.argv[2]

csv_filename = f'{work_dir}/cache_lat_{option}.csv'
png_filename = f'{work_dir}/cache_lat_{option}.png'
info_filename = f'{work_dir}/info.txt'

with open(info_filename, 'r') as file:
    info = file.read()

fig = plt.figure(figsize=(12, 5))
gs = gridspec.GridSpec(1, 2, width_ratios=[8, 2])
ax1 = plt.subplot(gs[0])
ax2 = plt.subplot(gs[1])

df = pd.read_csv(csv_filename)
columns = df.columns[1:]
for column in columns:
    ax1.plot(df['buffer_size'].values, df[column].values, label=column)

ax1.legend()
ax1.set_title(f'{option} cache latency vs size')
ax1.set_xlabel('Size(kb)')
ax1.set_ylabel('Latency(ns)')
ax2.text(0.5, 0.5, info, fontsize=12, ha='center', va='center')
ax2.axis('off')

plt.savefig(png_filename)
