import json
from collections import defaultdict
from statistics import geometric_mean
import numpy as np
import matplotlib.pyplot as plt

plt.rcParams["font.size"] = 12

import os
os.makedirs("plots", exist_ok=True)
output_pdf_all = "plots/overhead_crypto_all.pdf"
output_pdf_partial = "plots/partial.pdf"
output_pdf_selective = "plots/selective.pdf"

with open('result.json', 'r') as f:
    data = json.load(f)

reference_key = "origin"

target_keys = [
    "ASLH", "LightSLH", "FilterSLH_IR", "aSLH", "BaggyBranch", "stacking", "naive-baggy", "baggy-ASLH", "baggy-aSLH", "baggy-seq-spec", "stacking_sfi", "sfi-aSLH", "naive-sfi", "DP-original", "stackingDP", "naive-DP", "DP-ASLH", "DP-aSLH", "DP-seq", "DP-dual"
]

tool_name = "DualGuard"

name_transform = {
    "ASLH": "LLVM-SLH",
    "LightSLH": "LightSLH",
    "FilterSLH_IR": "SLH",
    "BaggyBranch": "Baggy",
    "stacking": "Baggy-stacking",
    "naive-baggy": "Baggy+SLH",
    "baggy-ASLH": "DualBaggy+SLH",
    "baggy-aSLH": "DualBaggy+SLH (Selective)",
    "baggy-seq-spec": tool_name + "-DualBaggy",
    "stacking_sfi": "SafeStack-stacking",
    "sfi-aSLH": tool_name + "-SafeStackSLH",
    "naive-sfi": "SafeStack+SLH",
    "DP-original": "DeltaPointers",
    "stackingDP": "DeltaPointers-stacking",
    "naive-DP": "DeltaPointers+SLH",
    "DP-ASLH": "DualDeltaPointers+SLH",
    "DP-aSLH": "DualDeltaPointers+SLH (Selective)",
    "DP-seq": "DP-seq",
    "DP-dual": tool_name + "-DualDeltaPointers",
    "aSLH": "aSLH (assembly)",
}

variants_to_plot_all = [
    "naive-baggy", "baggy-seq-spec", "naive-DP", "DP-dual", "naive-sfi", "sfi-aSLH"
]

variants_to_plot_partial = [
    "ASLH", "LightSLH", "BaggyBranch", "DP-original", "baggy-seq-spec", "DP-dual"
]

variants_to_plot_selective = [
    "baggy-ASLH", "baggy-aSLH", "DP-ASLH", "DP-aSLH", "naive-sfi", "sfi-aSLH"
]

ymax_all = 200
ymax_partial = 200
ymax_selective = 200

results = defaultdict(list)

avergae_overhead = {}

reduction_rate = {}

def calc_overhead_reduction():
    for benchmark, row in results.items():
        if row[0]["naive-baggy"] == 0.0 or row[0]["naive-sfi"] == 0.0 or row[0]["naive-DP"] == 0.0: 
            continue
        naive_baggy = row[0]["naive-baggy"]
        dual_baggy = row[0]["baggy-seq-spec"]
        naive_sfi = row[0]["naive-sfi"]
        dual_sfi = row[0]["sfi-aSLH"]
        naive_DP = row[0]["naive-DP"]
        dual_DP = row[0]["DP-dual"]
        print(f"{benchmark}: {naive_baggy:.2f}% -> {dual_baggy:.2f}%, {naive_sfi:.2f}% -> {dual_sfi:.2f}%, {naive_DP:.2f}% -> {dual_DP:.2f}%")
        baggy_reduction = (naive_baggy - dual_baggy) / naive_baggy * 100
        sfi_reduction = (naive_sfi - dual_sfi) / naive_sfi * 100
        dp_reduction = (naive_DP - dual_DP) / naive_DP * 100
        reduction_rate[benchmark[0]+"/"+benchmark[1]] = {
            "baggy": baggy_reduction,
            "sfi": sfi_reduction,
            "dp": dp_reduction
        }
    reduction_rate["Average"] = {
        "baggy": (avergae_overhead["naive-baggy"] - avergae_overhead["baggy-seq-spec"]) / avergae_overhead["naive-baggy"] * 100,
        "sfi": (avergae_overhead["naive-sfi"] - avergae_overhead["sfi-aSLH"]) / avergae_overhead["naive-sfi"] * 100,
        "dp": (avergae_overhead["naive-DP"] - avergae_overhead["DP-dual"]) / avergae_overhead["naive-DP"] * 100
    }


def calc_average_overhead():
    for method in target_keys:
        overheads = []
        for row in results.values():
            overheads.append((row[0][method] + 100 ) / 100)
        if overheads:
            avergae_overhead[method] = geometric_mean(overheads) * 100 - 100

def collect_results():
    for entry in data:
        key = (entry["benchmark"], entry["lib"], entry["size"])
        base = float(entry.get(reference_key, 0.0).split(",")[0])
        if base == 0.0:
            continue

        row = {}
        for method in target_keys:
            if method in entry:
                val = float(entry[method].split(",")[0])
                if val == -1:
                    print("Failed run for " + str(key) + " in variant " + method)
                    val = 0.1
                overhead = (val - base) / base * 100
                row[method] = overhead
        results[key].append(row)


def plot_all():
    plot_data = {}
    for (benchmark, lib, size), rows in results.items():
        merged = defaultdict(list)
        for row in rows:
            for method, overhead in row.items():
                merged[method].append(overhead)
        avg = {m: v[0] for m, v in merged.items()}
        label = f"{benchmark}\n{lib}"
        plot_data[label] = avg
    plot_data["Average\nOverhead"] = avergae_overhead


    benchmarks = list(plot_data.keys())
    n_bench = len(benchmarks)
    n_methods = len(variants_to_plot_all)
    x = np.arange(n_bench)

    width = 0.8 / n_methods

    plt.figure(figsize=(12, 4))

    colors = plt.get_cmap("tab20").colors

    for i, method in enumerate(variants_to_plot_all):
        values = [plot_data[b].get(method, 0.0) for b in benchmarks]
        capped_values = [max(min(v, ymax_all), 0) for v in values]
        
        if method == "naive-baggy" or method == "naive-DP":
            bars = plt.bar(x + i * width, capped_values, width,
                    label=name_transform[method], color="white",
                    edgecolor=colors[i % len(colors)], hatch='////')
        else:
            bars = plt.bar(x + i * width, capped_values, width,
                    label=name_transform[method], color=colors[i % len(colors)],
                    edgecolor='black')

        for bar, real_val in zip(bars, values):
            if real_val > ymax_all:
                plt.text(bar.get_x() + bar.get_width() / 2,
                        ymax_all + 1,
                        f"{real_val/100:.1f}x",
                        ha="center", va="bottom", fontsize=6, rotation=60)

    plt.ylabel("Overhead (%)", fontsize=10)
    plt.xticks(x + width * (n_methods - 1) / 2, benchmarks, rotation=45, ha="right", fontsize=10)
    plt.yticks(fontsize=8)
    plt.ylim(0, ymax_all)
    plt.legend(fontsize=8, ncol=2, loc='upper right', bbox_to_anchor=(0, 0, 0.53, 1))
    plt.grid(axis="y", linestyle="--", alpha=0.5)
    
    first_group_center = x[0] + (n_methods - 1) * width / 2
    last_group_center = x[-1] + (n_methods - 1) * width / 2

    plt.xlim(x[0] - 0.2, x[-1] + (n_methods * width))
    
    plt.tight_layout()

    plt.savefig(output_pdf_all, format="pdf", bbox_inches="tight")
    plt.close()
    print(f"✅ Saved capped combined plot as {output_pdf_all}")


# draw all benchmarks together
def plot_partial():
    plot_data = {}
    for (benchmark, lib, size), rows in results.items():
        merged = defaultdict(list)
        for row in rows:
            for method, overhead in row.items():
                merged[method].append(overhead)
        avg = {m: v[0] for m, v in merged.items()}
        label = f"{benchmark}\n{lib}"
        plot_data[label] = avg
    plot_data["Average\nOverhead"] = avergae_overhead

    benchmarks = list(plot_data.keys())
    n_bench = len(benchmarks)
    n_methods = len(variants_to_plot_partial)
    x = np.arange(n_bench)

    width = 0.8 / n_methods  # total group width adjustment

    plt.figure(figsize=(12, 4))

    # pick a readable color palette
    colors = plt.get_cmap("tab10").colors

    for i, method in enumerate(variants_to_plot_partial):
        values = [plot_data[b].get(method, 0.0) for b in benchmarks]
        capped_values = [max(min(v, ymax_partial), 0) for v in values]
        if method == "baggy-seq-spec" or method == "DP-dual":
            bars = plt.bar(x + i * width, capped_values, width,
                    label=name_transform[method], color=colors[i % len(colors)],
                    edgecolor='black')
        else:
            bars = plt.bar(x + i * width, capped_values, width,
                    label=name_transform[method], color="white",
                    edgecolor=colors[i % len(colors)], hatch='////')
        # Add value labels for clipped bars
        for bar, real_val in zip(bars, values):
            if real_val > ymax_partial:
                plt.text(bar.get_x() + bar.get_width() / 2,
                        ymax_partial + 1,
                        f"{real_val/100:.1f}x",
                        ha="center", va="bottom", fontsize=6, rotation=60)

    plt.ylabel("Overhead (%)", fontsize=12)
    plt.xticks(x + width * (n_methods - 1) / 2, benchmarks, rotation=45, ha="right", fontsize=10)
    plt.yticks(fontsize=8)
    plt.ylim(0, ymax_partial)
    plt.legend(fontsize=9, ncol=2, loc='upper right', bbox_to_anchor=(0, 0, 0.58, 1))
    plt.grid(axis="y", linestyle="--", alpha=0.5)
    
    first_group_center = x[0] + (n_methods - 1) * width / 2
    last_group_center = x[-1] + (n_methods - 1) * width / 2

    plt.xlim(x[0] - 0.2, x[-1] + (n_methods * width))
    
    plt.tight_layout()

    plt.savefig(output_pdf_partial, format="pdf", bbox_inches="tight")
    plt.close()
    print(f"✅ Saved capped combined plot as {output_pdf_partial}")
    
    
# draw selective effect
def plot_selective(name_transform):
    plot_data = {}
    for (benchmark, lib, size), rows in results.items():
        merged = defaultdict(list)
        for row in rows:
            for method, overhead in row.items():
                merged[method].append(overhead)
        avg = {m: v[0] for m, v in merged.items()}
        label = f"{benchmark}\n{lib}"
        plot_data[label] = avg
    plot_data["Average\nOverhead"] = avergae_overhead
    
    print("-------------------Average Overhead-----------------") 
    for method, overhead in avergae_overhead.items():
        print(f"{name_transform[method]:30s}: {overhead/100+1:7.2f}")
    print("----------------------------------------------------") 

    # Create single large grouped bar chart
    benchmarks = list(plot_data.keys())
    n_bench = len(benchmarks)
    n_methods = len(variants_to_plot_selective)
    x = np.arange(n_bench)

    width = 0.8 / n_methods  # total group width adjustment

    plt.figure(figsize=(12, 4))

    # pick a readable color palette
    colors = plt.get_cmap("tab20").colors

    for i, method in enumerate(variants_to_plot_selective):
        values = [plot_data[b].get(method, 0.0) for b in benchmarks]
        capped_values = [max(min(v, ymax_selective), 0) for v in values]
        
        bars = plt.bar(x + i * width, capped_values, width,
                    label=name_transform[method], color=colors[i % len(colors)],
                    edgecolor='black')
        
        for bar, real_val in zip(bars, values):
            if real_val > ymax_selective:
                plt.text(bar.get_x() + bar.get_width() / 2,
                        ymax_selective + 1,
                        f"{real_val/100:.1f}x",
                        ha="center", va="bottom", fontsize=6, rotation=60)

    plt.ylabel("Overhead (%)", fontsize=12)
    plt.xticks(x + width * (n_methods - 1) / 2, benchmarks, rotation=45, ha="right", fontsize=10)
    plt.yticks(fontsize=8)
    plt.ylim(0, ymax_selective)
    plt.legend(fontsize=9, ncol=2, loc='upper right', bbox_to_anchor=(0, 0, 0.58, 1))
    plt.grid(axis="y", linestyle="--", alpha=0.5)
    
    first_group_center = x[0] + (n_methods - 1) * width / 2
    last_group_center = x[-1] + (n_methods - 1) * width / 2

    plt.xlim(x[0] - 0.2, x[-1] + (n_methods * width))
    
    plt.tight_layout()

    plt.savefig(output_pdf_selective, format="pdf", bbox_inches="tight")
    plt.close()
    print(f"✅ Saved capped combined plot as {output_pdf_selective}")


if __name__ == "__main__":
    collect_results()
    
    calc_average_overhead()
    for method, overhead in avergae_overhead.items():
        print(f"{method:30s}: {overhead:7.2f}%")

    calc_overhead_reduction()
    for benchmark, rates in reduction_rate.items():
        print(f"{benchmark:30s}: Baggy Reduction: {rates['baggy']:6.2f}%, SFI Reduction: {rates['sfi']:6.2f}%, DP Reduction: {rates['dp']:6.2f}%")

    plot_all()
    plot_partial()
    name_transform_selective = name_transform
    name_transform_selective["sfi-aSLH"] = "SafeStack+SLH (Selective)"
    plot_selective(name_transform_selective)