import os
import subprocess
import csv
import re
import sys
import statistics

num_of_runs = 4
slh_stat_name = "Number of address mode used registers hardaned"
safestack_stat_name = "Number of unsafe static allocas"

# match_list = ['cactuBSSN_r', 'mcf_r', 'imagick_r', 'xz_r', 'povray_r', 'lbm_r', 'namd_r', 'deepsjeng_r', 'specrand_ir', 'omnetpp_r', 'parest_r', 'leela_r', 'x264_r', 'nab_r']
match_list = ['mcf_r', 'xz_r', 'lbm_r', 'namd_r', 'deepsjeng_r', 'nab_r']
# match_list = ['nab_r']

# the order matters
options = ['baseline', 'stacking-Baggy-branch-Aslh', 'hybrid-baggy-aslh', 'baggy-seq-spec', 'aslh', 'stacking-Safestack-Aslh', 'hybrid-safestack-aslh', 'Aslh', 'naive-safestack']

raw_csv = "raw_stats.csv"
summary_csv = "summary_stats.csv"
analysis_time_csv = "analysis_time.csv"

def collect_runtime(benchmark, data):
    full_path = "../logs/" + benchmark + ".log"
    benchmark_name = benchmark.split('-')[0]

    with open(full_path, "r") as log:
        success_line_pattern = r"Success: [0-9]+x[0-9]+\."+benchmark_name
        if not re.search(success_line_pattern, log.read()):
            print(f"Failed to run {benchmark}")
            for i in range(num_of_runs+2):
                data[benchmark].append(-1)
            return
        print(f"Successfully run {benchmark}")
        log.seek(0)
        for line in log:
            if 'The log for this run is in' in line:
                full_log = line.split('The log for this run is in')[1].strip()
                with open(full_log, mode='r', newline='') as full_log_file:
                    print(f"Processing {full_log}")
                    running_times = []
                    for log_line in full_log_file:
                        if 'Total elapsed time:' in log_line:
                            running_times.append(float(log_line.split('Total elapsed time:')[1].strip()))
                    running_time = sum(running_times) / len(running_times)
                    print(f"Running time: {running_time}")

                std_dev = statistics.pstdev(running_times)
                
                data[benchmark].extend(running_times)
                data[benchmark].append(std_dev)
                data[benchmark].append(running_time)


def collect_log_stats(base_dir, benchmark_name, suffix, data):
    log_suffix = suffix
    benchmark = benchmark_name + "-" + suffix
    for dir in os.listdir(base_dir):
        if benchmark_name in dir and os.path.isdir(os.path.join(base_dir, dir)):
            full_log_path = os.path.join(base_dir, dir, "build", "build_base_"+log_suffix+".0000", "log")

            if os.path.exists(full_log_path):
                safestack_pattern = r"\s*(\d+)\s+safe-stack\s+-\s+(.+)"
                safestack_data = {}
                baggy_pattern = r"Protect # of GEPs: ([0-9]+)"
                slh_pattern = r"\s*(\d+)\s+x86-slh\s+-\s+(.+)"
                running_time_pattern = r"Time taken: ([0-9]+) ms"
                slh_data = {}
                with open(full_log_path, "r") as log_file:
                    print(f"Processing {full_log_path}")
                    baggy_found = False
                    slh_found = False 
                    safestack_found = False
                    running_time_found = False
                    running_time = [-1, -1]
                    for line in log_file:
                        baggy_match = re.search(baggy_pattern, line)
                        if baggy_match:
                            baggy_found = True
                            baggy_count = int(baggy_match.group(1))
                            data[benchmark].append(baggy_count)
                        slh_match = re.match(slh_pattern, line)
                        if slh_match:
                            slh_found = True
                            count = int(slh_match.group(1))
                            stat_name = slh_match.group(2).strip()
                            slh_data[stat_name] = count
                        safestack_match = re.match(safestack_pattern, line)
                        if safestack_match:
                            safestack_found = True
                            count = int(safestack_match.group(1))
                            stat_name = safestack_match.group(2).strip()
                            safestack_data[stat_name] = count

                        running_time_match = re.search(running_time_pattern, line)
                        if running_time_match:
                            running_time_found = True
                            if running_time[0] == -1:
                                running_time[0] = float(running_time_match.group(1))
                            else:
                                running_time[1] = float(running_time_match.group(1))

                    if not baggy_found:
                        data[benchmark].append(-1)

                    if not slh_found:
                        data[benchmark].append({})
                    else:
                        data[benchmark].append(slh_data)

                    if not safestack_found:
                        data[benchmark].append({})
                    else:
                        data[benchmark].append(safestack_data)

                    if not running_time_found:
                        data[benchmark].append(-1)
                    else:
                        if running_time[1] == -1:
                            running_time = running_time[0]
                        else:
                            running_time = sum(running_time)
                        data[benchmark].append(running_time)
            else:
                print(f"Log file not found: {full_log_path}")
                data[benchmark].append(-1)
                data[benchmark].append({})



def write_raw(output_csv, results):
    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        header = ['Benchmark']
        for i in range(num_of_runs):
            header.append("#"+str(i+1)+" run")
        header.extend(['Std Dev', 'Avg', 'baggy', 'SLH', 'Safestack'])
        writer.writerow(header)  # Header
        for key, values in results.items():
            writer.writerow([key] + values)


def write_summary(output_csv, results):
    first_line = "Benchmark,Baseline-Baggy,,DualGuard-BaggySLH,,DualGuard-Baggy,DualGuard-SLH,Baseline-Safestack,,DualGuard-SafestackSLH,,DualGuard-SafestackSelSLH,,SLH"
    second_line = ",Baggy,SLH,Baggy,SLH,,,Safestack,SLH,Safestack,SLH,Safestack,SLH,"
    with open(output_csv, "w", newline="") as f:
        f.write(first_line + "\n")
        f.write(second_line + "\n")
        for benchmark in match_list:
            print(f"Writing summary for {benchmark}")
            line = benchmark + ","
            line += str(data[benchmark+"-"+options[1]][6]) + ","
            line += str(data[benchmark+"-"+options[1]][7][slh_stat_name]) + ","
            line += str(data[benchmark+"-"+options[2]][6]) + ","
            if slh_stat_name in data[benchmark+"-"+options[2]][7]:
                line += str(data[benchmark+"-"+options[2]][7][slh_stat_name]) + ","
            else:
                line += "0,"
            line += str(data[benchmark+"-"+options[3]][6]) + ","
            line += str(data[benchmark+"-"+options[4]][7][slh_stat_name]) + ","
            line += str(data[benchmark+"-"+options[5]][8][safestack_stat_name]) + ","
            line += str(data[benchmark+"-"+options[5]][7][slh_stat_name]) + ","

            line += str(data[benchmark+"-"+options[8]][8][safestack_stat_name]) + ","
            line += str(data[benchmark+"-"+options[8]][7][slh_stat_name]) + ","
            
            line += str(data[benchmark+"-"+options[6]][8][safestack_stat_name]) + ","
            line += str(data[benchmark+"-"+options[6]][7][slh_stat_name]) + ","
            line += str(data[benchmark+"-"+options[7]][7][slh_stat_name]) + "\n"
            f.write(line)


def write_running_time(output_csv, results):
    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        header = ['Benchmark']
        header.extend(['Running Time (s)'])
        writer.writerow(header)  # Header
        for benchmark in match_list:
            line = [benchmark]
            seconds = round(data[benchmark + "-" + options[2]][9]/1000, 2)
            line.append(seconds)
            writer.writerow(line)


if __name__ == "__main__":
    base_dir = "../benchspec/CPU"

    # data = {benchmark: [#1 run, ..., Std Dev, Avg, baggy, {slh1: count, slh2: count, ...}]}
    data = {}

    for benchmark in match_list:
        for option in options:
            print("+++++++++++++++++++++++++++++++++++++++++++++") 
            data[benchmark + "-" + option] = []
            collect_runtime(benchmark + "-" + option, data)
            collect_log_stats(base_dir, benchmark, option, data)

    write_raw(raw_csv, data)
    write_summary(summary_csv, data)
    write_running_time(analysis_time_csv, data)
