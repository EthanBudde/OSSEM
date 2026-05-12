# current function set
# compute_ylim
# compute_pressure_ylim
# finalize_entry
# parse_data_file
# format_time
# plot_data

# goal:
#   reduce the current processing flow for each cell of data
#       combining some functions, reducing logical abstraction to reduce function set
#   create more modular functions that can be called for two modes of operation
#       set graphing - visualizing a full set of data given as a file
#           sets are always full (always all sensors tracking concurrently)
#           can be reduced to single graphs or given as a full set (default)
#       live graphing - visualizing a live data stream
#           right now we're arbitrating what that live datastream is (files as datapackets sent via network integration)
#           live graphs will print single graphs or full graphs of the set (default)

# goalset
#   

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

metadata = []

# function: compute_ylim
# inputs  : series
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

# function: compute_pressure_ylim
# inputs  : series
# returns : min_v - pad
#           min_v + pad
def compute_pressure_ylim(series):
    # remove null entries
    clean = [v for v in series if v is not None]

    # failsafe for empty set
    if not clean:
        return (0, 1)

    # find boundaries
    min_v = min(clean)
    max_v = max(clean)

    # pad boundaries proportionally, return
    pad = (max_v - min_v) * 0.1 if max_v != min_v else 0.1
    return (min_v - pad, max_v + pad)


# remake or remove
# function: finalize_entry
# inputs  : current
#           last_bme
#           last_sgp
#           last_scd
# returns : current
#           last_bme
#           last_sgp
#           last_scd

def finalize_entry(current, last_bme, last_sgp, last_scd):
    # skip empty entry
    if current is None:
        return None, last_bme, last_sgp, last_scd

    # patch missing bme data
    if None in current["BMEvalues"]:
        current["BMEvalues"] = last_bme.copy()
    else:
        last_bme = current["BMEvalues"].copy()

    # patch missing sgp data
    if current["SGPvalues"] is None:
        current["SGPvalues"] = last_sgp.copy()
    else:
        last_sgp = current["SGPvalues"].copy()

    # patch missing scd data
    if None in current["SCDvalues"]:
        current["SCDvalues"] = last_scd.copy()
    else:
        last_scd = current["SCDvalues"].copy()

    # reject fully invalid packet
    if (
        None in current["BMEvalues"] or
        current["SGPvalues"] is None or
        None in current["SCDvalues"]
    ):
        return None, last_bme, last_sgp, last_scd

    return current, last_bme, last_sgp, last_scd

# function: parse_data_file
# inputs  : file_path
# outputs : data_container 
def parse_data_file(file_path):
    data_container = [] # generic data container

    # laggy sensor redundancy
    last_bme = [0, 0, 0, 0]
    last_sgp = [0, 0]
    last_scd = [0, 0, 0]

    # file parsing block
    try:
        with open(file_path, 'r') as file:

            # initialize packet state
            current = None
            current_block = None

            # LINE PARSING BEGIN
            for index, raw_line in enumerate(file):
                line = raw_line.strip()

                # skip blank lines
                if not line:
                    continue

                # parse metadata headers
                if line[0] == '#':
                    metadata.append(line[1:])
                    continue
                   
                if line[0] == '!':
                    timerflag = True
                else:
                    timerflag = False
                
                try:
                    # detect timestamp header
                    if ':' in line and '.' in line:

                        # close prior packet
                        finished, last_bme, last_sgp, last_scd = finalize_entry(
                            current,
                            last_bme,
                            last_sgp,
                            last_scd
                        )

                        if finished:
                            data_container.append(finished)

                        # split timestamp fields
                        hr, mn, rest = line.split(':')
                        sec, ms = rest.split('.')

                        # create new packet
                        current = {
                            "index": index,
                            "timestamp_hr": int(hr),
                            "timestamp_min": int(mn),
                            "timestamp_s": int(sec),
                            "timestamp_ms": int(ms),
                            "mark": timerflag,
                            "BMEvalues": [None] * 4,
                            "SCDvalues": [None] * 3,
                            "SGPvalues": None
                        }

                        # reset active block tracker
                        current_block = None
                        continue

                    # enter bme parsing block
                    if "BME BLOCK" in line:
                        current_block = "BME"
                        bme_idx = 0
                        continue

                    # enter scd parsing block
                    if "SCD BLOCK" in line:
                        current_block = "SCD"
                        scd_idx = 0
                        continue

                    # enter sgp parsing block
                    if "SGP BLOCK" in line:
                        current_block = "SGP"
                        sgp_vals = []
                        continue

                    # parse numeric sensor values
                    if '[' in line and ']' in line:
                        val = float(line.split('[')[0])

                        # assign bme values
                        if current_block == "BME":
                            current["BMEvalues"][bme_idx] = val
                            bme_idx += 1

                        # assign scd values
                        elif current_block == "SCD":
                            current["SCDvalues"][scd_idx] = val
                            scd_idx += 1

                        # assign sgp values
                        elif current_block == "SGP":
                            sgp_vals.append(val)
                            if len(sgp_vals) == 2:
                                current["SGPvalues"] = sgp_vals.copy()

                # ignore malformed lines
                except (ValueError, IndexError):
                    continue
            # LINE PARSING END

            # finalize last packet
            finished, last_bme, last_sgp, last_scd = finalize_entry(
                current,
                last_bme,
                last_sgp,
                last_scd
            )

            if finished:
                data_container.append(finished)

            timerflag = False    

    # ERROR CASE: file not found
    except FileNotFoundError:
        print("File not found.")

    # return packed data in container
    return data_container



# time format
def format_time(x, pos):
    return f"{int(x)}"

# function: plot_data
# inputs  : data
#           BMEcx
#           SCDcx
#           SGPcx
#           override
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
    mark_coords = [
        d['timestamp_min'] * 60 + d['timestamp_s'] + d['timestamp_ms']/1000.0
        for d in data if d['mark'] is true
        ]
    
    # establish start time, time coordinates array from data array
    start = time_coords[0]
    time_coords = [t - start for t in time_coords]
    mark_coords = [t - start for t in mark_coords]

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

        # x/y limits
        ax.set_ylim(ylim)
        ax.set_xlim(min(time_coords), max(time_coords))

        # ??
        ax.xaxis.set_major_formatter(FuncFormatter(format_time))

        # highlighting
        ax.axvspan(mark_coords[0], mark_coords[1], color='0.9')

    # ??disable graphs we don't want to see
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
