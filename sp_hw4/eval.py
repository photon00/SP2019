import numpy as np
import csv

ytest_bytes = open("hw4_data/y_test", 'rb').read()
y_test = np.array(bytearray(ytest_bytes)).astype(int)
y_eval = []
with open("result.csv") as f:
    reader = csv.reader(f)
    for i, r in enumerate(reader):
        if i == 0: continue
        y_eval.append(int(r[1]))

ans = np.equal(y_test, y_eval)
print("acc:　{}/{}({:.2%})".format(sum(ans), len(ans), sum(ans)/len(ans)))