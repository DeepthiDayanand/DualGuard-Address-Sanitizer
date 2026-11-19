import matplotlib.pyplot as plt
import csv
import numpy as np
import statistics

tool_name = "DualGuard"

benchmarks_to_plot = [
    'perlbench_r', 'mcf_r', 'namd_r', 'parest_r', 'povray_r', 'lbm_r', 'omnetpp_r', 'xalancbmk_r', 'x264_r', 'deepsjeng_r', 'leela_r', 'nab_r', 'xz_r', 'Average\nOverhead'
]

benchmarks_to_plot_selective = [
    'perlbench_r', 'mcf_r', 'namd_r', 'parest_r', 'povray_r', 'lbm_r', 'omnetpp_r', 'xalancbmk_r', 'x264_r', 'deepsjeng_r', 'leela_r', 'nab_r', 'xz_r', 'Average\nOverhead'
]

prefixes = {
    "mcf_r": "505",
    "xz_r": "557",
    "lbm_r": "519",
    "namd_r": "508",
    "deepsjeng_r": "531",
    "nab_r": "544",
    "perlbench_r": "500",
    "xalancbmk_r": "523",
    "x264_r": "525",
    "povray_r": "511",
}

running_time_file = "../stats.csv"

options_to_plot_all = ['baseline', 'naive-baggy', 'baggy-seq-spec', 'naive-deltapointers', 'deltapointers-dual', 'naive-safestack', 'hybrid-safestack-aslh']
options_to_plot_selective = ['baseline', 'hybrid-baggy-Aslh', 'hybrid-baggy-aslh', 'hybrid-deltapointers-Aslh', 'hybrid-deltapointers-aslh', 'naive-safestack', 'hybrid-safestack-aslh']

name_transfer = {
    'baseline': 'Origin',
    'stacking-Baggy-branch-Aslh': 'Baseline-Baggy',
    'hybrid-baggy-aslh': 'DualBaggy+SLH (Selective)',
    'hybrid-baggy-Aslh': 'DualBaggy+SLH',
    'baggy-seq-spec': tool_name + '-DualBaggy',
    'aslh': 'SLH',
    'stacking-Safestack-Aslh': 'Baseline-SafeStack',
    'hybrid-safestack-aslh': tool_name + '-SafeStackSLH',
    'hybrid-safestack-Aslh': 'SafeStack+SLH',
    'naive-baggy': 'Baggy+SLH',
    'naive-safestack': 'SafeStack+SLH',
    'naive-deltapointers': 'DeltaPointers+SLH',
    'deltapointers-dual': tool_name + '-DualDeltaPointers',
    'deltapointers-original': 'DualDeltaPointers',
    'hybrid-deltapointers-aslh': 'DualDeltaPointers+SLH (Selective)',
    'hybrid-deltapointers-Aslh': 'DualDeltaPointers+SLH'
}
compare_against = "Origin"

output_pdf_all = "overhead_spec_all.pdf"
output_pdf_selective = "overhead_spec_selective.pdf"

ymax_all = 200
ymax_selective = 200

overhead_reduction = {}

def calculate_overhead_reduction(data):
    for benchmark, times in data.items():
        if benchmark not in benchmarks_to_plot:
            continue
        overhead_reduction[benchmark] =  {
            "baggy": (times["Baggy+SLH"] - times["DualGuard-DualBaggy"]) / (times["Baggy+SLH"] - 1) * 100,
            "deltapointers": (times["DeltaPointers+SLH"] - times["DualGuard-DualDeltaPointers"]) / (times["DeltaPointers+SLH"] - 1) * 100,
            "safestack": (times["SafeStack+SLH"] - times["DualGuard-SafeStackSLH"]) / (times["SafeStack+SLH"] - 1) * 100,
        }

    for benchmark, reductions in overhead_reduction.items():
        print(f"{benchmark}: Baggy Reduction: {reductions['baggy']:.2f}%, SafeStack Reduction: {reductions['safestack']:.2f}%, DeltaPointers Reduction: {reductions['deltapointers']:.2f}%, ")


def calculate_average_overheads(data, benchmarks_to_plot=benchmarks_to_plot):
    average_overheads = {}
    overheads_by_opt = {}
    for benchmark, times in data.items():
        if benchmark not in benchmarks_to_plot:
            continue
        for opt, time in times.items():
            if opt not in overheads_by_opt:
                overheads_by_opt[opt] = []
            if time > 0:
                overheads_by_opt[opt].append(time)

    for opt, times in overheads_by_opt.items():
        average_overheads[opt] = statistics.geometric_mean(times)

    for opt, overhead in average_overheads.items():
        print(f"{opt}: {overhead:.2f}")
        
    return average_overheads


def plot_overhead_all(data):
    benchmarks = benchmarks_to_plot
    options = [name_transfer[opt] for opt in options_to_plot_all]
    options.remove(compare_against)
    colors = plt.get_cmap("tab20").colors

    # Compute the positions
    x = np.arange(len(benchmarks))
    bar_width = 1 / (len(options) + 1)

    plt.figure(figsize=(12, 4))
    # plt.title("Runtime Overhead", fontsize=14)
    plt.ylabel("Overhead (%)", fontsize=12)
    plt.xticks(x + bar_width * (len(options)-1) / 2, benchmarks, rotation=30, ha='right')
    plt.ylim(0, ymax_all)
    plt.grid(axis='y', linestyle='--', alpha=0.5)

    # Plot each configuration as a group
    for i, option in enumerate(options):
        heights = []
        values = []
        for bench in benchmarks:
            val = data[bench].get(option, 0)
            values.append((val - 1) * 100)
            heights.append(max(0, min((val - 1) * 100, ymax_all)))  # Convert normalized runtime to % overhead

        if option == "Baggy+SLH" or option == "DeltaPointers+SLH":
            bars = plt.bar(x + i * bar_width, heights, bar_width,
                           label=option, color="white", edgecolor=colors[i % len(colors)], hatch='////')
        else:                  
            bars = plt.bar(x + i * bar_width, heights, bar_width,
                    label=option, color=colors[i % len(colors)], edgecolor="black")

        # Optional: Add % labels above bars
        for rect, v, h in zip(bars, values, heights):
            if v == -100.0:
                plt.text(rect.get_x() + rect.get_width() / 2, h + 3,
                        "N/A", ha='center', va='bottom', fontsize=8, rotation=60)
            elif v > ymax_all:
                plt.text(rect.get_x() + rect.get_width() / 2, h + 2,
                        f"{v/100:.1f}x", ha='center', va='bottom', fontsize=8, rotation=60)

    plt.legend(loc='upper right', bbox_to_anchor=(0, 0, 0.23, 1), fontsize=9)
    plt.tight_layout()
    plt.savefig(output_pdf_all, format="pdf", bbox_inches="tight")
    plt.close()


def plot_overhead_selective(data, name_transfer):
    benchmarks = benchmarks_to_plot_selective
    options = [name_transfer[opt] for opt in options_to_plot_selective]
    options.remove(compare_against)
    colors = plt.get_cmap("tab20").colors

    # Compute the positions
    x = np.arange(len(benchmarks))
    bar_width = 1 / (len(options) + 1)

    plt.figure(figsize=(12, 4))
    # plt.title("Runtime Overhead", fontsize=14)
    plt.ylabel("Overhead (%)", fontsize=12)
    plt.xticks(x + bar_width * (len(options)-1) / 2, benchmarks, rotation=30, ha='right')
    plt.ylim(0, ymax_all)
    plt.grid(axis='y', linestyle='--', alpha=0.5)

    # Plot each configuration as a group
    for i, option in enumerate(options):
        heights = []
        values = []
        for bench in benchmarks:
            val = data[bench].get(option, 0)
            values.append((val - 1) * 100)
            heights.append(max(0, min((val - 1) * 100, ymax_all)))  # Convert normalized runtime to % overhead
            
        bars = plt.bar(x + i * bar_width, heights, bar_width,
                       label=option, color=colors[i % len(colors)], edgecolor="black")

        # Optional: Add % labels above bars
        for rect, v, h in zip(bars, values, heights):
            if v == -100.0:
                plt.text(rect.get_x() + rect.get_width() / 2, h + 3,
                        "N/A", ha='center', va='bottom', fontsize=8, rotation=60)
            elif v > ymax_all:
                plt.text(rect.get_x() + rect.get_width() / 2, h + 2,
                        f"{v/100:.1f}x", ha='center', va='bottom', fontsize=8, rotation=60)

    plt.legend(loc='upper right', bbox_to_anchor=(0, 0, 0.25, 1), fontsize=9)
    plt.tight_layout()
    plt.savefig(output_pdf_selective, format="pdf", bbox_inches="tight")
    plt.close()


def collect_running_times(options, name_transfer=name_transfer):
    data = {}
    with open(running_time_file, mode='r', newline='') as csvfile:
        reader = csv.reader(csvfile)
        next(reader)  # Skip header
        for row in reader:
            bench_opt = row[0]
            time = float(row[1])
            benchmark = bench_opt.split('-')[0]
            option = bench_opt[len(benchmark)+1:]
            if option not in options:
                continue
            option = name_transfer[option]
            if benchmark not in data:
                data[benchmark] = {}
            
            data[benchmark][option] = time
            
    for benchmark, times in data.items():
        for option in options:
            if name_transfer[option] not in times:
                data[benchmark][name_transfer[option]] = 0

    return data

def normalize_overhead(data):
    normalized_data = {}
    for benchmark, times in data.items():
        baseline_time = times[compare_against]
        normalized_data[benchmark] = {}
        for opt in times:
            if opt == compare_against:
                continue
            normalized_data[benchmark][opt] = times[opt] / baseline_time
    return normalized_data


if __name__ == "__main__":
    print("Ploting all overheads...")
    running_time_data_all = collect_running_times(options_to_plot_all)
    normalized_data_all = normalize_overhead(running_time_data_all)
    average_overhead_all = calculate_average_overheads(normalized_data_all)
    
    with open("detailed_overhead.csv", mode='w') as csvfile:
        writer = csv.writer(csvfile)
        header = ['Benchmark']
        header.extend(options_to_plot_all)
        writer.writerow(header)  # Header
        for benchmark, times in normalized_data_all.items():
            line = [benchmark]
            for opt in options_to_plot_all:
                time = times.get(name_transfer[opt], -1)
                line.append(time)
            writer.writerow(line)
            
        line = ["Average\nOverhead"]
        for opt in options_to_plot_all:
            time = average_overhead_all.get(name_transfer[opt], -1)
            line.append(time)
        writer.writerow(line)
    
    normalized_data_all["Average\nOverhead"] = average_overhead_all
    calculate_overhead_reduction(normalized_data_all)
    plot_overhead_all(normalized_data_all)
    
    print("Ploting selective overheads...")
    name_transfer_selective = name_transfer
    name_transfer_selective['hybrid-safestack-aslh'] = "SafeStack+SLH (Selective)"
    
    running_time_data_selective = collect_running_times(options_to_plot_selective, name_transfer_selective)
    normalized_data_selective = normalize_overhead(running_time_data_selective)
    average_overhead_selective = calculate_average_overheads(normalized_data_selective, benchmarks_to_plot_selective)
    normalized_data_selective["Average\nOverhead"] = average_overhead_selective
     
    plot_overhead_selective(normalized_data_selective, name_transfer_selective)