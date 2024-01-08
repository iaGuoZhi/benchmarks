import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import sys

script = sys.argv[0]

if not len(sys.argv) == 2:
    print('Usage: python {script} work_dir')
    sys.exit(1)
work_dir = sys.argv[1]

ratio_csv_filename = f'{work_dir}/compress_ratio.csv'
com_thr_csv_filename = f'{work_dir}/compress_throughput.csv'
decom_thr_csv_filename = f'{work_dir}/decompress_throughput.csv'
info_filename = f'{work_dir}/info.txt'
ratio_png_filename = f'{work_dir}/compress_ratio.png'
com_thr_png_filename = f'{work_dir}/compress_throughput.png'
decom_thr_png_filename = f'{work_dir}/decompress_throughput.png'

#with open(info_filename, 'r') as file:
#    info = file.read()

# Draw ratio plot
df = pd.read_csv(ratio_csv_filename)
fig, ax = plt.subplots()
bar_width = 0.2
index = df.index
opacity = 0.8
labels = df['work']

for i, col in enumerate(df.columns[1:]):
    ax.bar(index + i * bar_width, df[col], bar_width, alpha=opacity, label=col)

ax.set_xlabel('work')
ax.set_ylabel('Compression Ratio')
ax.set_title('Compression Ratio by Compression algorithm and Dataset')
ax.set_xticks(index + bar_width)
ax.set_xticklabels(labels)
ax.legend()
plt.savefig(ratio_png_filename)

# Draw compress throughput plot
df = pd.read_csv(com_thr_csv_filename)
fig, ax = plt.subplots()
bar_width = 0.2
index = df.index
opacity = 0.8
labels = df['work']

for i, col in enumerate(df.columns[1:]):
    ax.bar(index + i * bar_width, df[col], bar_width, alpha=opacity, label=col)

ax.set_xlabel('work')
ax.set_ylabel('Compression throughput (MB/s)')
ax.set_title('Compression throughput by Compression algorithm and Dataset')
ax.set_xticks(index + bar_width)
ax.set_xticklabels(labels)
ax.legend()
plt.savefig(com_thr_png_filename)

# Draw decompress throughput plot
df = pd.read_csv(decom_thr_csv_filename)
fig, ax = plt.subplots()
bar_width = 0.2
index = df.index
opacity = 0.8
labels = df['work']

for i, col in enumerate(df.columns[1:]):
    ax.bar(index + i * bar_width, df[col], bar_width, alpha=opacity, label=col)

ax.set_xlabel('work')
ax.set_ylabel('Decompression throughput (MB/s)')
ax.set_title('Decompression throughput by Compression algorithm and Dataset')
ax.set_xticks(index + bar_width)
ax.set_xticklabels(labels)
ax.legend()
plt.savefig(decom_thr_png_filename)
