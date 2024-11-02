from model import Net
import torch

model = Net()

model.load_state_dict(torch.load('model.pth'))

# Extract the weights and biases
weights = []
biases = []
for layer in model.layers:
    if isinstance(layer, torch.nn.Linear):
        weights.append(layer.weight.detach().numpy())
        biases.append(layer.bias.detach().numpy())

# Save weights and biases as C++ code
with open('model_params.hpp', 'w') as f:
    f.write("#pragma once\n\n")
    f.write("#include <vector>\n\n")
    f.write("struct ModelParams\n{\n")

    for i, (weight, bias) in enumerate(zip(weights, biases)):
        print(weight.shape, bias.shape)
        f.write(f"// Layer {i + 1}\n")
        f.write(
            f"const std::vector<std::vector<float>> weights_{i} = {{\n")
        for j in range(weight.shape[0]):
            f.write("    {")
            for k in range(weight.shape[1]):
                f.write(f"{weight[j][k]:.20f}f")
                if k < weight.shape[1] - 1:
                    f.write(", ")
                else:
                    f.write("}")
            if j < weight.shape[0] - 1:
                f.write(",\n")
            else:
                f.write("\n};\n\n")
        f.write(
            f"const std::vector<float> biases_{i} = {{")
        for j in range(bias.shape[0]):
            f.write(f"{bias[j]:.20f}f")
            if j < bias.shape[0] - 1:
                f.write(", ")
            else:
                f.write("};\n\n")
    f.write("};\n")
