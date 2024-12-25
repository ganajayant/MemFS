#include "MemFS.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

class CpuUsage {
public:
    double total_jiffies;
    double work_jiffies;
    CpuUsage(double total_jiffies, double work_jiffies) : total_jiffies(total_jiffies), work_jiffies(work_jiffies) {}
};

CpuUsage *get_cpu_usage() {
    ifstream statusFile("/proc/stat");
    if (!statusFile.is_open()) {
        cerr << "Failed to open /proc/stat" << endl;
        return nullptr;
    }
    string line;
    getline(statusFile, line);
    statusFile.close();

    istringstream iss(line);
    string cpuLabel;
    long user, nice, system, idle, iowait, irq, softirq, steal;

    iss >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    if (cpuLabel != "cpu") {
        cerr << "Unexpected format in /proc/stat" << endl;
        return nullptr;
    }
    long total_jiffies = user + nice + system + idle + iowait + irq + softirq + steal;
    long work_jiffies = user + nice + system;
    return new CpuUsage(total_jiffies, work_jiffies);
}

long get_memory_usage_kb() {
    ifstream statusFile("/proc/self/status");
    if (!statusFile.is_open()) {
        cerr << "Failed to open /proc/self/status" << endl;
        return -1;
    }

    string line;
    while (getline(statusFile, line)) {
        if (line.find("VmRSS:") == 0) {
            istringstream iss(line);
            string label;
            long memory_kb;
            iss >> label >> memory_kb;
            statusFile.close();
            return memory_kb;
        }
    }
    statusFile.close();
    cerr << "VmRSS not found in /proc/self/status" << endl;
    return -1;
}

string random_string() {
    string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    random_shuffle(str.begin(), str.end());
    return str.substr(0, 10);
}

int main() {
    vector<int> threads = {1,
                           2,
                           4,
                           8,
                           16};
    vector<int> work_loads = {
        100,
        1000,
        10000};

    ofstream out_file;
    // if the out_file exist remove it and create a new one
    out_file.open("benchmark.txt", ios::out | ios::trunc);
    if (!out_file.is_open()) {
        cerr << "Failed to open benchmark.txt" << endl;
        return 1;
    }

    for (int thread : threads) {
        for (int work_load : work_loads) {
            MemFS fs{static_cast<size_t>(thread)};
            vector<string> filenames;
            vector<string> contents;
            for (int i = 0; i < 10000; ++i) {
                filenames.push_back("file" + to_string(i) + ".txt");
            }
            for (int i = 0; i < 10000; ++i) {
                contents.push_back(random_string());
            }

            CpuUsage *start_cpu = get_cpu_usage();
            // get the intial memory usage

            auto start_create = chrono::high_resolution_clock::now();
            fs.create_files(10000, filenames);
            auto duration_create = chrono::high_resolution_clock::now() - start_create;

            auto start_write = chrono::high_resolution_clock::now();
            fs.write_files(10000, filenames, contents);
            auto duration_write = chrono::high_resolution_clock::now() - start_write;

            auto start_read = chrono::high_resolution_clock::now();
            for (int i = 0; i < 10000; ++i) {
                fs.read_file("file" + to_string(i) + ".txt");
            }
            auto duration_read = chrono::high_resolution_clock::now() - start_read;

            auto start_delete = chrono::high_resolution_clock::now();
            fs.delete_files(10000, filenames);
            auto duration_delete = chrono::high_resolution_clock::now() - start_delete;

            CpuUsage *end_cpu = get_cpu_usage();
            this_thread::sleep_for(chrono::seconds(1));
            long memory_usage = get_memory_usage_kb();

            double cpu_utilization = (end_cpu->work_jiffies - start_cpu->work_jiffies) / (end_cpu->total_jiffies - start_cpu->total_jiffies) * 100;

            out_file << "Benchmarking with " << thread << " threads and " << work_load << " work load" << endl;
            out_file << "Create: " << chrono::duration_cast<chrono::milliseconds>(duration_create).count() << "ms" << endl;
            out_file << "Write: " << chrono::duration_cast<chrono::milliseconds>(duration_write).count() << "ms" << endl;
            out_file << "Read: " << chrono::duration_cast<chrono::milliseconds>(duration_read).count() << "ms" << endl;
            out_file << "Delete: " << chrono::duration_cast<chrono::milliseconds>(duration_delete).count() << "ms" << endl;
            out_file << "CPU Utilization: " << cpu_utilization << "%" << endl;
            out_file << "Memory Usage: " << memory_usage << "KB" << endl;
            out_file << endl;
        }
    }
    return 0;
}
