import csv
import os
import sys
import re

if len(sys.argv) != 2:
    print("Usage: python collecting_stats.py <benchmark_name>")
    sys.exit(1)

name = sys.argv[1]
log_file = "logs/" + name + '.log'
output_csv = 'stats.csv'

print(f'Processing {log_file}')

if not os.path.isfile(output_csv):
    with open(output_csv, mode='w', newline='') as csv_file:
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(['Benchmark', 'Running Time'])

existing_rows = []
with open(output_csv, mode='r', newline='') as csv_file:
    csv_reader = csv.reader(csv_file)
    existing_rows = list(csv_reader)

updated = False

with open(log_file, 'r') as file:
    success = False
    benchmark_name = name.split('-')[0]
    success_line_pattern = r"Success: [0-9]+x[0-9]+\."+benchmark_name
    if not re.search(success_line_pattern, file.read()):
        print(f"Failed to run {name}")
        sys.exit(1)
    print(f"Successfully run {name}")
    file.seek(0)
    for line in file:
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
            for row in existing_rows:
                if row[0] == name:
                    row[1] = running_time
                    updated = True
                    break
            if not updated:
                existing_rows.append([name, running_time])
            break

with open(output_csv, mode='w', newline='') as csv_file:
    csv_writer = csv.writer(csv_file)
    csv_writer.writerows(existing_rows)
