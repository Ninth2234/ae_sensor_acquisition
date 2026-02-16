import numpy as np
import matplotlib.pyplot as plt

# read raw uint16 data
data = np.fromfile("data_plot/adc_log.bin", dtype=np.uint16)

print("Samples:", len(data))
print("First 10 samples:", data[:10])

# plot
plt.plot(data)
plt.xlabel("Sample index")
plt.ylabel("ADC value")
plt.show()
