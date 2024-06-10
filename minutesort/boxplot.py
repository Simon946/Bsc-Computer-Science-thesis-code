import matplotlib.pyplot as plt
import numpy as np

# Data
data_a = [360, 361, 372, 362, 369]
data_b = [161, 159, 160, 166, 160]
data_c = [75, 74, 74, 73, 75]
data_d = [36, 36, 37, 37, 37]
data_e = [18, 18, 19, 18, 18]

# Create a list of data for the boxplots
data_list = [data_a, data_b, data_c, data_d, data_e]

# Create the boxplots
plt.boxplot(data_list, labels=["1", "2", "4", "8", "16"])
plt.xlabel("Number of workers")
plt.ylabel("Execution time (s)")
plt.title("Execution time for different number of workers")
plt.grid(False)
plt.show()
