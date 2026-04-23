import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

metadata = []

# function: compute_ylim
# inputs:   series
#           trim ratio          default 0.05
#           margin ratio        default 0.1
#           extra pad ratio     default 0.05, 
def compute_auto_ylim(series, trim_ratio=0.05, margin_ratio=0.1, extra_pad_ratio=0.05):
    # makes worker array of values in series input  
    clean = [v for v in series if v is not None]

    # if short, return
    if len(clean) < 5:
        return (0, 1)

    # remove large value variance
    start_idx = int(len(clean) * trim_ratio) # integer of (length of clean) * trim ratio
    trimmed = clean[start_idx:] # start clean @ trimmed start point

    # calculate average of dataset, largest standard deviation 
    avg = sum(trimmed) / len(trimmed) 
    dev = max(abs(v - avg) for v in trimmed)

    margin = dev * (1 + margin_ratio) # margin of y ratio = standard deviation scaled by margin ratio

    # padding 
    low = avg - margin
    high = avg + margin
    pad = (high - low) * extra_pad_ratio
    low -= pad
    high += pad

    # prevent padding from dying
    if abs(high - low) < 1:
        low -= 0.5
        high += 0.5

    return (low, high)

# compute_pressure_ylim
# 
def compute_pressure_ylim(series):
    clean = [v for v in series if v is not None]
    if not clean:
        return (0, 1)

    min_v = min(clean)
    max_v = max(clean)

    pad = (max_v - min_v) * 0.1 if max_v != min_v else 0.1
    return (min_v - pad, max_v + pad)


# remake or remove
def finalize_entry(current, last_sgp, last_scd):
    if current is None:
        return None, last_sgp, last_scd

    if current["SGPvalues"] is None:
        current["SGPvalues"] = last_sgp.copy()
    else:
        last_sgp = current["SGPvalues"].copy()

    if None in current["SCDvalues"]:
        current["SCDvalues"] = last_scd.copy()
    else:
        last_scd = current["SCDvalues"].copy()

    if None in current["BMEvalues"]:
        return None, last_sgp, last_scd

    return current, last_sgp, last_scd

# organize and comment the shit out of
def parse_data_file(file_path):
    data_container = [] # generic data container

    # laggy sensor redundancy
    last_bme = [0, 0, 0, 0]
    last_sgp = [0, 0]
    last_scd = [0, 0, 0]

    # file parsing block
    try:
        with open(file_path, 'r') as file:

            # 
            current = None
            current_block = None

            # LINE PARSING BEGIN
            for index, raw_line in enumerate(file):
                line = raw_line.strip()

                if not line:
                    continue

                if line[0] == '#':
                    metadata.append(line[1:])
                    continue
                #
                try:
                    if ':' in line and '.' in line:

                        #
                        finished, last_sgp, last_scd = finalize_entry(current, last_sgp, last_scd)
                        if finished:
                            data_container.append(finished)

                        hr, mn, rest = line.split(':')
                        sec, ms = rest.split('.')

                        #
                        current = {
                            "index": index,
                            "timestamp_hr": int(hr),
                            "timestamp_min": int(mn),
                            "timestamp_s": int(sec),
                            "timestamp_ms": int(ms),
                            "BMEvalues": [None]*4,
                            "SCDvalues": [None]*3,
                            "SGPvalues": None
                        }

                        current_block = None
                        continue

                    #
                    if "BME BLOCK" in line:
                        current_block = "BME"
                        bme_idx = 0
                        continue

                    if "SCD block" in line:
                        current_block = "SCD"
                        scd_idx = 0
                        continue

                    if "SGP BLOCK" in line:
                        current_block = "SGP"
                        sgp_vals = []
                        continue

                    if "{SCDEND}" in line:
                        current_block = None
                        continue

                    #
                    if '[' in line and ']' in line:
                        val = float(line.split('[')[0])

                        #
                        if current_block == "BME":
                            current["BMEvalues"][bme_idx] = val
                            bme_idx += 1

                        elif current_block == "SCD":
                            current["SCDvalues"][scd_idx] = val
                            scd_idx += 1

                        elif current_block == "SGP":
                            sgp_vals.append(val)
                            if len(sgp_vals) == 2:
                                current["SGPvalues"] = sgp_vals.copy()

                except (ValueError, IndexError):
                    continue
            # LINE PARSING END

            # 
            finished, last_sgp, last_scd = finalize_entry(current, last_sgp, last_scd)
            if finished:
                data_container.append(finished)

    # ERROR CASE: file not found
    except FileNotFoundError:
        print("File not found.")

    # return packed data in container
    return data_container


# time format
def format_time(x, pos):
    return f"{int(x)}"

# 
def plot_data(data, BMEcx, SCDcx, SGPcx, override):
    # strip empty ?
    data = [d for d in data if d is not None]

    # ERROR CHECK: if no data, exit graphing subroutine
    if not data:
        print("No valid data.")
        return

    # calculate timestamps for printing
    time_coords = [
        d['timestamp_min'] * 60 + d['timestamp_s'] + d['timestamp_ms']/1000.0
        for d in data
    ]

    # establish start time, time coordinates array from data array
    start = time_coords[0]
    time_coords = [t - start for t in time_coords]

    # labels
    # todo: move somewhere more overtly declared
    labels = [
        "BME Temp (°C)", "BME Pressure (hPa)", "BME Humidity (%)", "BME Gas (KΩ)",
        "SCD Temp (°C)", "SCD Humidity (%)", "SCD CO2 (ppm)",
        "SGP TVOC (ppb)", "SGP CO2 (ppm)"
    ]

    # series labels corresponding with input style
    # alphabetical order on sensors is canonical
    series = [
        [d['BMEvalues'][0] for d in data],
        [d['BMEvalues'][1] for d in data],
        [d['BMEvalues'][2] for d in data],
        [d['BMEvalues'][3] for d in data],
        [d['SCDvalues'][0] for d in data],
        [d['SCDvalues'][1] for d in data],
        [d['SCDvalues'][2] for d in data],
        [d['SGPvalues'][0] for d in data],
        [d['SGPvalues'][1] for d in data],
    ]

    # enabled sensor data packages for printing
    enabled = []

    # enable and append that set, based on inputs
    for i, bit in enumerate(BMEcx):
        if bit == '1':
            enabled.append(i)

    for i, bit in enumerate(SCDcx):
        if bit == '1':
            enabled.append(i + 4)

    for i, bit in enumerate(SGPcx):
        if bit == '1':
            enabled.append(i + 7)

    # ?
    if override > 0:
        enabled = [override - 1]

    graphAmnt = len(enabled)
    if graphAmnt == 0:
        return

    # default 
    cols = 3
    rows = (graphAmnt + cols - 1) // cols

    fig, axes = plt.subplots(rows, cols, figsize=(12, 4*rows), constrained_layout=True)
    axes = axes.flatten()

    fig.suptitle(metadata[0], fontsize=16)
    fig.text(0.02, 0.97, 'data gathered: ' + metadata[1], fontsize=10)
    fig.text(0.98, 0.97, 'data graphed: ' + metadata[2], fontsize=10, ha='right')

    # plot enabled graps
    for i, idx in enumerate(enabled):
        ax = axes[i]

        ax.plot(time_coords, series[idx])
        ax.set_title(labels[idx])
        ax.set_xlabel("Time (s)")

        # 
        if idx == 1:
            ylim = compute_pressure_ylim(series[idx])
        else:
            ylim = compute_auto_ylim(series[idx])

        ax.set_ylim(ylim)
        ax.set_xlim(min(time_coords), max(time_coords))
        ax.xaxis.set_major_formatter(FuncFormatter(format_time))

    for j in range(graphAmnt, len(axes)):
        axes[j].set_visible(False)

    plt.savefig("test.png", dpi=300)
    plt.show()




# -------- RUN --------
data = parse_data_file('file.txt')

BME = format(15, "04b")
SCD = format(7, "03b")
SGP = format(3, "02b")

override = 0

plot_data(data, BME, SCD, SGP, override)
