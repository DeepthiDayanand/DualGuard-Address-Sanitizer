import subprocess
import re
import sys

def run_benchmark(executable, num_iterations, min_time, warmup_iterations, cpu_affinity):
    warmup_command = f'taskset -c {cpu_affinity} {executable} --benchmark_repetitions={warmup_iterations} --benchmark_min_time={min_time}'
    

    subprocess.run(warmup_command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


    benchmark_command = f'taskset -c {cpu_affinity} {executable} --benchmark_repetitions={num_iterations} --benchmark_min_time={min_time}'

    result = subprocess.run(benchmark_command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    return result.stdout

def extract_median_time(output, benchname):
    pattern_raw = rf'{benchname}\s+([\d\.]+)\s+ns'
    pattern = rf'{benchname}_median\s+([\d\.]+)\s+ns'
    pattern_stddev = rf'{benchname}_stddev\s+([\d\.]+)\s+ns'
    match = re.search(pattern, output)
    match_stddev = re.search(pattern_stddev, output)

    median = -1
    stddev = -1
    if match:
        median = float(match.group(1))

    if match_stddev:
        stddev = float(match_stddev.group(1))

    return median, stddev

def main():
    if len(sys.argv) != 7:
        print("Usage: python3 script.py <executable_name> <bench_name> <num_iterations> <min_time> <warmup_iterations> <cpu_affinity>")
        sys.exit(1)
    
    executable = sys.argv[1]   
    benchname = sys.argv[2]    
    num_iterations = int(sys.argv[3])  
    min_time = float(sys.argv[4])     
    warmup_iterations = int(sys.argv[5])  
    cpu_affinity = sys.argv[6]  

    output = run_benchmark(executable, num_iterations, min_time, warmup_iterations, cpu_affinity)

    median_time_ns, stddev_time_ns = extract_median_time(output, benchname)

    if median_time_ns is not None:
        print(f'{median_time_ns:.2f}, {stddev_time_ns:.2f}', end='')
    else:
        print('No valid median time extracted.')

if __name__ == '__main__':
    main()
