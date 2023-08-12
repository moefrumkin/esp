import random


WIDTH = 1024

weights = []
inputs = []

for i in range(WIDTH):
    weights.append([])
    inputs.append([])
    for j in range(WIDTH):
        weights[i].append(random.randint(-4, 4))
        inputs[i].append(random.randint(-4, 4))


outputs = [] * WIDTH

print("starting")

for i in range(WIDTH):
    outputs.append([])
    for j in range(WIDTH):
        outputs[i].append(0)
        for k in range(WIDTH):
            outputs[i][j] += weights[i][k] * inputs[k][j]

print("saving weights")

print(weights)
print(inputs)
print(outputs)

def write_pref(pref, mat):
    counter = 0
    f = open(pref + ".h", "w")
    for row in mat:
        for item in row:
            f.write(f"{pref}[{counter}] = {item};\n")
            counter += 1

    f.close()

write_pref("weights", weights)

print("weights done")

write_pref("inputs", inputs)

print("inputs done")

write_pref("outputs", outputs)
