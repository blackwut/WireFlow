import argparse


def get_frequencies(text):

    lines = text.split("\n")
    frequencies = []
    for line in lines:
        if "Compute Unit:" in line:
            compute_unit = line.split("Compute Unit: ")[1]
        if "Achieved Kernel Clock Frequency:" in line:
            frequency = float(line.split("Achieved Kernel Clock Frequency: ")[1].split(" ")[0])
            frequencies.append((compute_unit, frequency))
    return frequencies


def get_resources(text):
    lines = text.split("\n")
    resources = []
    for line in lines:
        if "Compute Unit:" in line:
            compute_unit = line.split("Compute Unit: ")[1]
        if "LUT " in line:
            lut_slr0 = float(line.split("|")[2])
            lut_slr1 = float(line.split("|")[3])
        # if "LUTAsLogic " in line:
        #     lut_as_logic_slr0 = float(line.split("|")[2])
        #     lut_as_logic_slr1 = float(line.split("|")[3])
        # if "LUTAsMem " in line:
        #     lut_as_mem_slr0 = float(line.split("|")[2])
        #     lut_as_mem_slr1 = float(line.split("|")[3])
        if "REG " in line:
            reg_slr0 = float(line.split("|")[2])
            reg_slr1 = float(line.split("|")[3])
        # if "CARRY8 " in line:
        #     carry8_slr0 = float(line.split("|")[2])
        #     carry8_slr1 = float(line.split("|")[3])
        # if "F7MUX " in line:
        #     f7mux_slr0 = float(line.split("|")[2])
        #     f7mux_slr1 = float(line.split("|")[3])
        # if "F8MUX " in line:
        #     f8mux_slr0 = float(line.split("|")[2])
        #     f8mux_slr1 = float(line.split("|")[3])
        # if "F9MUX " in line:
        #     f9mux_slr0 = float(line.split("|")[2])
        #     f9mux_slr1 = float(line.split("|")[3])
        if "BRAM " in line:
            bram_slr0 = float(line.split("|")[2])
            bram_slr1 = float(line.split("|")[3])
        if "URAM " in line:
            uram_slr0 = float(line.split("|")[2])
            uram_slr1 = float(line.split("|")[3])
        if "DSPs " in line:
            dsps_slr0 = float(line.split("|")[2])
            dsps_slr1 = float(line.split("|")[3])
            resources.append(
                (
                    compute_unit,
                    lut_slr0 + lut_slr1,
                    reg_slr0 + reg_slr1,
                    bram_slr0 + bram_slr1,
                    uram_slr0 + uram_slr1,
                    dsps_slr0 + dsps_slr1
                )
            )
        if "6. Compute Unit Utilization per SLR" in line:
            break
    return resources

def get_resource_sum(resources):

    lut = 0
    reg = 0
    bram = 0
    uram = 0
    dsps = 0
    for r in resources:
        lut += r[1]
        reg += r[2]
        bram += r[3]
        uram += r[4]
        dsps += r[5]
    return (lut, reg, bram, uram, dsps)


# parser = argparse.ArgumentParser()
# parser.add_argument("filename", help="path to the report file")
# args = parser.parse_args()
# with open(args.filename, "r") as f:
#     text = f.read()
# frequencies = get_frequencies(text)
# for compute_unit, frequency in frequencies:
#     print(f"{compute_unit}: {frequency}")

# resources = get_resources(text)
# print("+" + ("-" * 16 + "+") * 6)
# print("".join([f"|{r:16}" for r in ["Compute Unit", "LUT", "REG", "BRAM", "URAM", "DSPs"]]))
# print("+" + ("-" * 16 + "+") * 6)
# for resource in resources:
#     print("".join([f"|{r:16}" for r in resource]))


pars = (1, 2, 3, 4, 5)
apps = {
    "sds": {
        "full_name": "SpikeDetection",
        "operators": 4,
        "max_par": 6
    },
    "sdlc": {
        "full_name": "SpikeDetection",
        "operators": 4,
        "max_par": 6
    },
    "sdtc": {
        "full_name": "SpikeDetection",
        "operators": 4,
        "max_par": 6
    },
    "sdlh": {
        "full_name": "SpikeDetection",
        "operators": 4,
        "max_par": 5
    },
    "sdth": {
        "full_name": "SpikeDetection",
        "operators": 4,
        "max_par": 5
    },
    # "fds": {
    #     "full_name": "FraudDetection",
    #     "operators": 3,
    #     "max_par": 6
    # },
    "fdc": {
        "full_name": "FraudDetection",
        "operators": 3,
        "max_par": 6
    },
    "fdh": {
        "full_name": "FraudDetection",
        "operators": 3,
        "max_par": 5
    },
    # "ffds": {
    #     "full_name": "FraudFreqDetection",
    #     "operators": 4,
    #     "max_par": 6
    # }
    # "ffdc": {
    #     "full_name": "FraudFreqDetection",
    #     "operators": 4,
    #     "max_par": 6
    # }
    # "ffdh": {
    #     "full_name": "FraudFreqDetection",
    #     "operators": 4,
    #     "max_par": 5
    # }
}

# USE THIS FOR THE FINAL REPORT
# pars = (1, 2, 3, 4, 5, 6)
# apps = {
#     "sds": {
#         "full_name": "SpikeDetection",
#         "operators": 4
#     },
#     "fds": {
#         "full_name": "FraudDetection",
#         "operators": 3
#     },
#     "ffds": {
#         "full_name": "FraudFreqDetection",
#         "operators": 4
#     }
# }

def print_latex(par, frequencies, resources):
    min_freq = min([f[1] for f in frequencies])

    print(app, end="")
    print(f" & {par}", end="")
    for r in resources:
        print(f" & {int(r)}", end="")
    print(" & 1", end="") # II
    print(f" & {int(min_freq)}", end="") # min frequency TODO: check if this is correct
    print(f" & {par * int(min_freq)}", end="")
    print(" \\\\ ")

def print_table(par, frequencies, resources):
    min_freq = min([f[1] for f in frequencies])

    print(f"{app:8}", end="")
    print(f"|{par:8}", end="")
    for r in resources:
        print(f"|{int(r):8}", end="")
    print(f"|{1:8}", end="") # II
    print(f"|{int(min_freq):8}", end="") # min frequency TODO: check if this is correct
    print(f"|{par * int(min_freq):8}", end="")
    print("|")


for app in apps:
    for par in range(1, apps[app]['max_par'] + 1):
        filename = f"../Applications/{apps[app]['full_name']}/{app}/kernels/{app}"
        for i in range(apps[app]["operators"]):
            filename += str(par)
        filename += "/hw/report/link/automation_summary.txt"

        with open(filename, "r") as f:
            text = f.read()
        frequencies = get_frequencies(text)
        resources = get_resource_sum(get_resources(text))

        print_table(par, frequencies, resources)
        # print_latex(par, frequencies, resources)