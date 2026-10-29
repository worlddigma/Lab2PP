#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <limits.h>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <cctype>

using namespace std;
using namespace std::chrono;

enum class PopResult { Got, Empty, Finished };

class Queue {
public:
    void push(uint64_t v) {
        lock_guard<mutex> lk(m);
        q.push(v);
        cv.notify_one();
    }
    void close() {
        lock_guard<mutex> lk(m);
        done = true;
        cv.notify_all();
    }
    PopResult pop(uint64_t& v) {
        unique_lock<mutex> lk(m);
        cv.wait(lk, [this] { return !q.empty() || done; });
        if (!q.empty()) {
            v = q.front();
            q.pop();
            return PopResult::Got;
        }
        return PopResult::Finished;
    }
private:
    queue<uint64_t> q;
    mutex m;
    condition_variable cv;
    bool done = false;
};

bool isPrime(uint64_t n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (uint64_t d = 3; d * d <= n; d += 2)
        if (n % d == 0) return false;
    return true;
}

// reader: читает числа из общего файла, кладёт в свою очередь
void reader(ifstream& in, mutex& in_mtx, Queue& q,
    int reader_delay_ms, int id) {
    while (true) {
        uint64_t x;
        {
            lock_guard<mutex> lk(in_mtx);
            if (!(in >> x)) break; // EOF или ошибка
        }
        // эмуляция скорости чтения
        if (reader_delay_ms > 0)
            this_thread::sleep_for(milliseconds(reader_delay_ms));
        q.push(x);
    }
    q.close();
    cout << "[Reader " << id << "] завершил чтение\n";
}

// worker: извлекает числа, проверяет на простоту, пишет результат
void worker(Queue& q, const string& out_name,
    int worker_delay_ms, int id) {
    ofstream out(out_name);
    if (!out) {
        cerr << "Не удалось открыть " << out_name << "\n";
        return;
    }
    while (true) {
        uint64_t x;
        auto r = q.pop(x);
        if (r == PopResult::Finished) break;
        // эмуляция скорости вычисления
        if (worker_delay_ms > 0)
            this_thread::sleep_for(milliseconds(worker_delay_ms));
        out << x << " " << (isPrime(x) ? "да" : "нет") << "\n";
    }
    out.close();
    cout << "[Worker " << id << "] завершил работу\n";
}

bool isInteger(const string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!isdigit(c)) return false;
    return true;
}

int main() {
    setlocale(LC_ALL, "Russian");
    string file_path;
    string pairs_str;
    cout << "Введите путь к файлу: ";
    getline(cin, file_path);
    cout << "Введите количество пар потоков: ";
    getline(cin, pairs_str);

    int n_pairs = 2;
    if (isInteger(pairs_str)) {
        try {
            long v = stol(pairs_str);
            if (v > 0) n_pairs = (int)v;
            else cout << "Число должно быть > 0, берём 2\n";
        }
        catch (...) {
            cout << "Не удалось разобрать число, берём 2\n";
        }
    }
    else {
        cout << "Не целое число, берём 2\n";
    }

    ifstream in(file_path);
    if (!in) {
        cerr << "Невозможно открыть файл " << file_path << "\n";
        return 1;
    }

    mutex in_mtx;
    vector<thread> threads;
    vector<Queue*> queues;

    // Задержки для моделирования ситуаций 
    int reader_delay_ms = 10;
    int worker_delay_ms = 10;

    for (int i = 0; i < n_pairs; ++i) {
        Queue* q = new Queue();
        queues.push_back(q);
        string out_name = "out_" + to_string(i) + ".txt";
        threads.emplace_back(reader, ref(in), ref(in_mtx), ref(*q),
            reader_delay_ms, i);
        threads.emplace_back(worker, ref(*q), out_name,
            worker_delay_ms, i);
    }

    for (auto& t : threads) t.join();
    for (auto* q : queues) delete q;

    cout << "Готово.\n";
    return 0;
}