#ifndef MEMFS_HPP
#define MEMFS_HPP

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <ostream>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

class File {
public:
    system_clock::time_point created_at;
    system_clock::time_point updated_at;
    string content;

    File() : created_at(system_clock::now()), updated_at(created_at) {}

    size_t getSize() const {
        return sizeof(File) + content.size() * sizeof(char);
    }

    string getCreatedTime() const {
        auto time = system_clock::to_time_t(created_at);
        string time_str = ctime(&time);
        if (!time_str.empty() && time_str[time_str.length() - 1] == '\n') {
            time_str.erase(time_str.length() - 1);
        }
        return time_str;
    }

    string getUpdatedTime() const {
        auto time = system_clock::to_time_t(updated_at);
        string time_str = ctime(&time);
        if (!time_str.empty() && time_str[time_str.length() - 1] == '\n') {
            time_str.erase(time_str.length() - 1);
        }
        return time_str;
    }
};

class Task {
public:
    string filename;
    string content;
    string type;
    Task() = default;
    Task(string filename, string content, string type) : filename(filename), content(content), type(type) {}
};

class MemFS {
private:
    void worker_function() {
        while (true) {
            Task task{};
            {
                unique_lock<mutex> lock(task_queue_mutex);
                task_queue_cv.wait(lock, [this]() { return !task_queue.empty() || shutdown; });

                if (shutdown && task_queue.empty()) {
                    break;
                }

                task = task_queue.front();
                task_queue.pop();
            }
            if (task.type == "create") {
                create_file(task.filename);
            } else if (task.type == "write") {
                write_file(task.filename, task.content);
            } else if (task.type == "delete") {
                delete_file(task.filename);
            }

            {
                lock_guard<mutex> lock(task_queue_mutex);
                --outstanding_tasks;
                if (outstanding_tasks == 0) {
                    task_completion_cv.notify_all();
                }
            }
        }
    }
    bool create_file(const string &filename) {
        lock_guard<mutex> lock(files_mutex);
        if (files.find(filename) != files.end()) {
            cout << "error: another file with same name exists" << endl;
            return false;
        }
        files[filename] = File();
        return true;
    }

    bool write_file(const string &filename, const string &content) {
        lock_guard<mutex> lock(files_mutex);
        if (files.find(filename) == files.end()) {
            cout << "Error: " << filename << " does not exist" << endl;
            return false;
        }
        if (files[filename].getSize() + content.length() > 2048) {
            cout << "Error: " << filename << " has reached the maximum size of 2048 bytes" << endl;
            return false;
        }
        files[filename].content += content;
        files[filename].updated_at = system_clock::now();
        return true;
    }

    bool delete_file(const string &filename) {
        lock_guard<mutex> lock(files_mutex);
        if (files.find(filename) == files.end()) {
            cout << "File " << filename << " doesn't exist.";
            return false;
        }
        files.erase(filename);
        return true;
    }

public:
    map<string, File> files;
    mutable mutex files_mutex;
    size_t thread_count;
    queue<Task> task_queue;
    mutex task_queue_mutex;
    condition_variable task_queue_cv;

    size_t outstanding_tasks = 0;
    condition_variable task_completion_cv;
    atomic<bool> shutdown{false};
    vector<thread> workers;

    MemFS(size_t thread_count) : thread_count(thread_count) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers.emplace_back(&MemFS::worker_function, this);
        }
    }

    ~MemFS() {
        {
            lock_guard<mutex> lock(task_queue_mutex);
            shutdown = true;
        }
        task_queue_cv.notify_all();
        for (thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void create_files(int number_of_files, const vector<string> &filenames) {
        if (number_of_files == 1) {
            bool flag = create_file(filenames[0]);
            if (flag) {
                cout << "file created successfully" << endl;
            }
            return;
        }
        set<string> unique_files;
        {
            lock_guard<mutex> lock(task_queue_mutex);
            for (const auto &filename : filenames) {
                if (unique_files.find(filename) != unique_files.end()) {
                    cout << "error: another file with same name exists" << endl;
                    continue;
                }
                task_queue.push(Task(filename, "", "create"));
                ++outstanding_tasks;
                unique_files.insert(filename);
            }
            task_queue_cv.notify_all();
        }

        unique_lock<mutex> lock(task_queue_mutex);
        task_completion_cv.wait(lock, [this]() { return outstanding_tasks == 0; });

        cout << "files created successfully" << endl;
    }

    void write_files(int number_of_files, const vector<string> &filenames, const vector<string> &contents) {
        if (number_of_files == 1) {
            bool flag = write_file(filenames[0], contents[0]);
            if (flag) {
                cout << "successfully written to " << filenames[0] << endl;
            }
            return;
        }

        {
            lock_guard<mutex> lock(task_queue_mutex);
            for (int i = 0; i < number_of_files; ++i) {
                task_queue.push(Task(filenames[i], contents[i], "write"));
                ++outstanding_tasks;
            }
            task_queue_cv.notify_all();
        }

        unique_lock<mutex> lock(task_queue_mutex);
        task_completion_cv.wait(lock, [this]() { return outstanding_tasks == 0; });
        cout << "successfully written to the given files" << endl;
    }

    void read_file(const string &filename) {
        lock_guard<mutex> lock(files_mutex);
        if (files.find(filename) == files.end()) {
            cout << "Error: " << filename << " does not exist" << endl;
            return;
        }
        cout << files[filename].content << endl;
    }

    void delete_files(int number_of_files, const vector<string> &filenames) {
        if (number_of_files == 1) {
            bool flag = delete_file(filenames[0]);
            if (flag) {
                cout << "file deleted successfully" << endl;
            }
            return;
        }
        {
            lock_guard<mutex> lock(task_queue_mutex);
            for (const auto &filename : filenames) {
                task_queue.push(Task(filename, "", "delete"));
                ++outstanding_tasks;
            }
            task_queue_cv.notify_all();
        }

        unique_lock<mutex> lock(task_queue_mutex);
        task_completion_cv.wait(lock, [this]() { return outstanding_tasks == 0; });

        cout << "files deleted successfully" << endl;
    }

    void ls(bool lflag) {
        if (!lflag) {
            lock_guard<mutex> lock(files_mutex);
            for (auto &file : files) {
                cout << file.first << endl;
            }
            return;
        }
        lock_guard<mutex> lock(files_mutex);
        const int MY_SIZE_WIDTH = 10;
        const int TIME_WIDTH = 30;
        const int NAME_WIDTH = 20;

        cout << left
             << setw(MY_SIZE_WIDTH) << "size"
             << setw(TIME_WIDTH) << "created"
             << setw(TIME_WIDTH) << "last modified"
             << setw(NAME_WIDTH) << "filename"
             << endl;

        cout << string(MY_SIZE_WIDTH + TIME_WIDTH * 2 + NAME_WIDTH, '-') << endl;

        for (auto &file : files) {
            cout << left
                 << setw(MY_SIZE_WIDTH) << file.second.getSize()
                 << setw(TIME_WIDTH) << file.second.getCreatedTime()
                 << setw(TIME_WIDTH) << file.second.getUpdatedTime()
                 << setw(NAME_WIDTH) << file.first
                 << endl;
        }
    }
};
#endif
