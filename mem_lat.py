import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import sys
import csv
from scipy.signal import find_peaks

script = sys.argv[0]

if not len(sys.argv) == 3:
    sys.exit(f'Usage: python3 {script} input_csv_filename output_png_filename')

csv_filename = sys.argv[1]
png_filename = sys.argv[2]

df = pd.read_csv(csv_filename)

columns = df.columns[1:]

for column in columns:
    plt.plot(df['buffer_size'], df[column], label=column)

plt.legend()
plt.title('Latency vs Size in different stride')
plt.xlabel('Size')
plt.ylabel('Latency')

plt.savefig(png_filename)

plt.show()

