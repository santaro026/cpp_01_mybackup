#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <functional>
#include <memory>
#include <cstring>
#include <chrono>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif


using namespace std;
namespace fs = std::filesystem;

const fs::path ROOT = fs::path(getenv("HOME")) / "220_cpp/01_mybackup";

class FileReader {
private:
    ifstream file;
public:
    FileReader(const string& filename) : file(filename) {
        if (!file.is_open()) {
            throw runtime_error("failed to open file");
        }
    }
    string readAll() {
        string content;
        file.seekg(0, ios::end);
        content.reserve(file.tellg());
        file.seekg(0, ios::beg);
        content.assign(
            (istreambuf_iterator<char>(file)),
            istreambuf_iterator<char>()
        );
        return content;
    }
    ~FileReader() {
        if (file.is_open()) {
            file.close();
        }
    }
};

class LineReader {
private:
    ifstream file;
    vector<string> lines;
public:
    LineReader(const string& filename) : file(filename) {
        if (!file.is_open()) {
            throw runtime_error("failed to open file");
        }
    }
    void processLineByLine(function<void(const string&)> processor) {
        string line;
        while (getline(file, line)) {
            if (line.empty()) continue;
            processor(line);
        }
        file.clear();
        file.seekg(0);
    }
    vector<string> findLines(const string& searchStr) {
        vector<string> matches;
        processLineByLine([&](const string& line) {
            if (line.find(searchStr) != string::npos) {
                matches.push_back(line);
            }
        });
        return matches;
    }
};

template<typename T>
class BinaryReader {
private:
    ifstream file;
public:
    BinaryReader(const string& filename) : file(filename, ios::binary) {
        if (!file.is_open()) {
            throw runtime_error("failed to open binary file");
        }
    }
    T readFixed() {
        T data;
        file.read(reinterpret_cast<char*>(&data), sizeof(T));
        if (file.fail()) {
            throw runtime_error("failed to load data");
        }
        return data;
    }
    vector<T> readArray(size_t count) {
        vector<T> data(count);
        file.read(reinterpret_cast<char*>(data.data()), count*sizeof(T));
        if (file.fail()) {
            throw runtime_error("failed to load array");
        }
        return data;
    }
    size_t getFileSize() {
        file.seekg(0, ios::end);
        size_t size = file.tellg();
        file.seekg(0, ios::beg);
        return size;
    }
};

struct Record {
    int id; // 4 byte
    double value; // 8 byte
    // int value; // 4 byte
    char name[50]; // 50 byte
};

class MemoryMappedFile {
private:
    #ifdef _WIN32
        HANDLE fileHandle = INVALID_HANDLE_VALUE;
        HANDLE mappingHandle = INVALID_HANDLE_VALUE;
    #else
        int fd = -1;
    #endif
    void* mappedData = nullptr;
    size_t fileSize = 0;
public:
    MemoryMappedFile(const string& filename) {
        #ifdef _WIN32
            fileHandle = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SAHRE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (fileHandle == INVALID_HANDLE_VALUE) {
                throw runtime_error("failed to open file");
            }
            LARGE_INTEGER size:
            if (!GetFileSizeEx(fileHandle, &size)) {
                CloseHandle(fileHandle);
                throw runtime_error("failed to get file size");
            }
            fileSize = size.QuadPart;
            mappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (mappingHandle == INVALID_HANDLE_VALUE) {
                CloseHandle(fileHandle);
                throw runtime_error("failed to make file mapping");
            }
            mappedData = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
        #else
            fd = open(filename.c_str(), O_RDONLY);
            if (fd == -1) {
                throw runtime_error("failed to open file");
            }
            struct stat sb;
            if (fstat(fd, &sb) == -1) {
                close(fd);
                throw runtime_error("failed to get file information");
            }
            fileSize = sb.st_size;
            mappedData = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mappedData == MAP_FAILED) {
                close(fd);
                throw runtime_error("failed to map memory");
            }
        #endif
    }

    const char* getData() const { return static_cast<const char*>(mappedData); }
    size_t getSize() const { return fileSize; }

    ~MemoryMappedFile() {
        #ifdef _WIN32
            if (mappedData) UnmapViewOfFile(mappedData);
            if (mappingHandle != INVALID_HANDLE_VALUE) CloseHandle(mappingHandle);
            if (fileHandle != INVALID_HANDLE_VALUE) CloseHandle(fileHandle);
        #else
            if (mappedData != MAP_FAILED) munmap(mappedData, fileSize);
            if (fd != -1) close(fd);
        #endif
    }
};

class BufferedReader {
private:
    ifstream file;
    vector<char> buffer;
    size_t bufferSize;
    size_t position;
    size_t dataInBuffer;
public:
    BufferedReader(const string& filename, size_t bufferSize=8192) :
        file(filename, ios::binary),
        buffer(bufferSize),
        bufferSize(bufferSize),
        position(0),
        dataInBuffer(0)
        {
            if (!file.is_open()) {
                throw runtime_error("failed to open file");
            }
        }
    size_t read(char* data, size_t size) {
        size_t totalBytesRead = 0;
        while (totalBytesRead < size) {
            if (position >= dataInBuffer) {
                file.read(buffer.data(), bufferSize);
                dataInBuffer = file.gcount();
                position = 0;
                if (dataInBuffer == 0) break;
            }
            size_t bytesToCopy = min(size-totalBytesRead, dataInBuffer-position);
            memcpy(data + totalBytesRead, buffer.data() + position, bytesToCopy);
            position += bytesToCopy;
            totalBytesRead += bytesToCopy;
        }
        return totalBytesRead;
    }
};

// struct FileInfo {
//     enum class Type {Directory, File, Other};
//     Type type;
//     string timestamp;
//     uintmax_t size;
//     fs::path path;
//     int max_depth = 0;
//     FileInfo() = default;
//     FileInfo(const fs::path& p) : path(p), size(0) {
//         error_code ec;
//         fs::directory_entry entry(p, ec);
//         if (ec) {
//             type = Type::Other;
//             timestamp = "N/A";
//         }
//         if (entry.is_directory()) {
//             type = Type::Directory;
//             size = get_dir_size(p);
//             auto [_depth, _t, _d, _f, _o] = get_max_depth(p);
//             max_depth = _depth;
//         } else if (entry.is_regular_file(ec)) {
//             type = Type::File;
//             size = fs::file_size(p, ec);
//             max_depth = -1;
//         } else {
//             type = Type::Other;
//             max_depth = -2;
//         }
//         timestamp = get_last_write_time(entry);
//     }

//     void print(ostream& os) const {
//         string typeStr;
//         switch (type) {
//             case Type::Directory: typeStr = "[DIR]  "; break;
//             case Type::File: typeStr = "[FILE] " ; break;
//             case Type::Other: typeStr = "[OTHER]"; break;
//         }
//         os << typeStr << timestamp << static_cast<double>(size)/1'000'000 << " [MB] " << path << endl;
//     }

//     void writeToFile(const string& filename) const {
//         ofstream ofs(filename, ios::app);
//         if (!ofs) {
//             throw runtime_error("cannot open file: " + filename);
//         }
//         print(ofs);
//     }
// };


tuple<string, chrono::system_clock::time_point> get_last_write_time(const fs::directory_entry& entry) {
    fs::file_time_type ftime = entry.last_write_time();
    chrono::system_clock::time_point sctp = chrono::clock_cast<chrono::system_clock>(ftime);
    time_t cftime = chrono::system_clock::to_time_t(sctp);
    // tm* tm = localtime(&cftime); // non thred safe
    tm tm_buf{};
    #ifdef _WIN32
        if (localtime_s(&tm_buf, &cftime) != 0) {
            return {"N/A", stcp};
        }
    #else
        if (localtime_r(&cftime, &tm_buf) == nullptr) {
            return {"N/A", sctp};
        }
    #endif
    // c style
    // char buf[64];
    // size_t len = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    // return {string(buf, len), sctp};
    // cpp style using string
    string buf(64, '\0');
    size_t len = strftime(&buf[2], buf.size(), "%Y-%m-%d %H:%M:%S", &tm_buf);
    buf.resize(len);
    return {buf, sctp};
}

void get_last_write_time(const fs::directory_entry& entry, string& timestamp, chrono::system_clock::time_point& sctp) {
    error_code ec;
    fs::file_time_type ftime = fs::last_write_time(entry.path(), ec);
    // fs::file_time_type ftime = entry.last_write_time();
    if (ec) {
        cerr << "get_last_write_time failed: " << entry.path() << endl << "ec.message: " << ec.message() << endl;
        timestamp = "";
    } else {
        sctp = chrono::clock_cast<chrono::system_clock>(ftime);
        time_t cftime = chrono::system_clock::to_time_t(sctp);
        tm tm_buf{};
        #ifdef _WIN32
            if (localtime_s(&tm_buf, &cftime) != 0) {
                return;
            }
        #else
            if (localtime_r(&cftime, &tm_buf) == nullptr) {
                return;
            }
        #endif
        string buf(64, '\0');
        size_t len = strftime(&buf[2], buf.size(), "%Y-%m-%d %H:%M:%S", &tm_buf);
        buf.resize(len);
        timestamp = buf;
    }
}

uintmax_t get_dirsize(const fs::path& root) {
    error_code ec;
    uintmax_t dirsize = 0;
    int max_depth = 0;
    size_t num_childs = 0;
    size_t num_childs_dir = 0;
    size_t num_childs_file = 0;
    size_t num_childs_other = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                cerr << "permission denied: " << entry.path() << endl;
                ec.clear();
                continue;
            }
            if (entry.is_regular_file()) {
                auto _size = fs::file_size(entry, ec);
                if (!ec) {
                    dirsize += _size;
                } else {
                    cerr << "permission denied (file size): " << entry.path() << endl;
                    ec.clear();
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        cout << "Error: " << e.what() << endl;
    }
    return dirsize;
}

tuple<uintmax_t, size_t, size_t, size_t, size_t, size_t> get_dirstatistic(const fs::path& root) {
    error_code ec;
    uintmax_t dirsize = 0;
    size_t max_depth = 0;
    size_t num_childs = 0;
    size_t num_childs_dir = 0;
    size_t num_childs_file = 0;
    size_t num_childs_other = 0;
    try {
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; ++it) {
            const fs::directory_entry& entry = *it;
            if (ec) {
                cerr << "permission denied: " << entry.path() << endl;
                ec.clear();
                continue;
            }
            size_t depth = static_cast<size_t>(it.depth());
            if (depth > max_depth) max_depth = depth;
            if (entry.is_directory()) {
                num_childs_dir++;
            } else if (entry.is_regular_file()) {
                num_childs_file++;
                uintmax_t _size = fs::file_size(entry, ec);
                if (!ec) {
                    dirsize += _size;
                } else {
                    cerr << "permission denied (file size): " << entry.path() << endl;
                    ec.clear();
                }
            } else {
                num_childs_other++;
            }
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "Error: " << e.what() << endl;
    }
    num_childs = num_childs_dir + num_childs_file + num_childs_other;
    return {dirsize, max_depth, num_childs, num_childs_dir, num_childs_file, num_childs_other};
}

struct ChildInfo {
    enum class Type {Directory, File, Other};
    Type type;
    int depth = -1;
    chrono::system_clock::time_point sctp;
    string timestamp;
    uintmax_t size = 0;
    fs::path path;
    fs::path root;
    ChildInfo() = default;
    ChildInfo(const fs::path r, const fs::path& p) : root(r), path(p) {
        error_code ec;
        fs::directory_entry entry(p, ec);
        if (entry.is_directory()) {
            type = ChildInfo::Type::Directory;
            fs::path rel = fs::relative(path, root);
            for (const auto& part : rel) {
                depth++;
            }
            get_last_write_time(entry, timestamp, sctp);
        } else if (entry.is_regular_file()) {
            type = ChildInfo::Type::File;
            fs::path rel = fs::relative(path, root);
            for (const auto& part : rel) {
                depth++;
            }
            get_last_write_time(entry, timestamp, sctp);
            size = fs::file_size(entry, ec);
            if (ec) {
                cerr << "permission denied (file size): " << entry.path() << endl;
                ec.clear();
            }
        } else {
            type = ChildInfo::Type::Other;
            fs::path rel = fs::relative(path, root);
            for (const auto& part : rel) {
                depth++;
            }
            get_last_write_time(entry, timestamp, sctp);
        }
    }

    static string to_string(Type t) {
        switch(t) {
            case Type::File: return "[F]";
            case Type::Directory: return "[D]";
            case Type::Other: return "[O]";
            default: return "[U]";
        }
    }
};

struct DirInfo {
    enum class Type {Directory, File, Other};
    Type type;
    chrono::system_clock::time_point sctp;
    string timestamp;
    uintmax_t size = 0;
    fs::path path;
    int max_depth = 0;
    size_t num_childs_recursive = 0;
    size_t num_childs_dir_recursive = 0;
    size_t num_childs_file_recursive = 0;
    size_t num_childs_other_recursive = 0;
    size_t num_child = 0;
    size_t num_child_dir = 0;
    size_t num_child_file = 0;
    size_t num_child_other = 0;
    vector<DirInfo> childs_nested;
    vector<ChildInfo> childs;

    static int type_priority(Type t) {
        switch (t) {
            case Type::File: return 0;
            case Type::Directory: return 1;
            case Type::Other: return 2;
        }
        return 3;
    }

    void sort_childs_nested() {
        sort(childs_nested.begin(), childs_nested.end(),
        [](const DirInfo& a, const DirInfo& b) {
            int pa = type_priority(a.type);
            int pb = type_priority(b.type);
            if (pa != pb) return pa < pb;
            return a.path < b.path;
        });
    }

    static int type_priority(ChildInfo::Type t) {
        switch (t) {
            case ChildInfo::Type::File: return 0;
            case ChildInfo::Type::Directory: return 1;
            case ChildInfo::Type::Other: return 2;
        }
        return 3;
    }

    void sort_childs() {
        sort(childs.begin(), childs.end(),
        [](const ChildInfo& a, const ChildInfo& b) {
            int pa = type_priority(a.type);
            int pb = type_priority(b.type);
            if (a.depth != b.depth) return a.depth < b.depth;
            if (pa != pb) return pa < pb;
            if (a.sctp != b.sctp) return a.sctp < b.sctp;
            return a.path < b.path;
        });
    }

    DirInfo() = default;
    DirInfo(const fs::path& p) : path(p) {
        error_code ec;
        fs::directory_entry entry(p, ec);
        int explored_depth = 0;
        if (ec) {
            type = Type::Other;
            timestamp = "N/A";
            string space(16, ' ');
            timestamp = timestamp + space;
        }
        if (entry.is_directory()) {
            type = Type::Directory;
            num_childs_dir_recursive++;
            int _depth;
            for (fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec), end; it != end; ++it) {
                const fs::directory_entry& entry = *it;
                if (ec) {
                    cerr << "permission deneid: " << entry.path() << endl;
                    ec.clear();
                    continue;
                }
                _depth = static_cast<int>(it.depth());
                if (_depth > max_depth) max_depth = _depth;
                childs.emplace_back(path, entry.path());
                if (entry.is_regular_file()) {
                    num_childs_file_recursive++;
                } else if (entry.is_directory()) {
                    num_childs_dir_recursive++;
                } else {
                    num_childs_other_recursive++;
                }
            }
        sort_childs();
        num_childs_recursive = num_childs_dir_recursive + num_childs_file_recursive + num_childs_other_recursive;
        num_child = num_child_dir + num_child_file + num_child_other;
        } else if (entry.is_regular_file(ec)) {
            type = Type::File;
            size = fs::file_size(p);
            max_depth = -1;
        } else {
            type = Type::Other;
            max_depth = -2;
        }
        if (entry.exists()) {
            const auto [_timestamp, _sctp] = get_last_write_time(entry);
            timestamp = _timestamp;
            sctp = _sctp;
        } else {
            timestamp = "N/A";
            string space(16, ' ');
            timestamp = timestamp + space;
        }
    }

    DirInfo(const fs::path& p, int recurse_depth) : path(p) {
        error_code ec;
        fs::directory_entry entry(p, ec);
        int explored_depth = 0;
        if (ec) {
            type = Type::Other;
            timestamp = "N/A";
            string space(16, ' ');
            timestamp = timestamp + space;
        }
        if (entry.is_directory()) {
            type = Type::Directory;
            auto [_size, _max_depth, _num_childs_recursive, _num_childs_dir_recursive, _num_childs_file_recursive, _num_childs_other_recursive] = get_dirstatistic(p);
            size = _size;
            max_depth = _max_depth;
            num_childs_recursive = _num_childs_recursive;
            num_childs_dir_recursive = _num_childs_dir_recursive;
            num_childs_file_recursive = _num_childs_file_recursive;
            num_childs_other_recursive = _num_childs_other_recursive;
            if (explored_depth<=recurse_depth) {
                try {
                    for (const auto& e : fs::directory_iterator(p)) {
                        if (e.is_directory()) {
                            num_child_dir++;
                            childs_nested.emplace_back(e.path(), recurse_depth-explored_depth-1);
                        } else if (e.is_regular_file()) {
                            num_child_file++;
                            childs_nested.emplace_back(e.path(), -1);
                        } else {
                            num_child_other++;
                            childs_nested.emplace_back(e.path(), -1);
                        }
                    }
                } catch (const fs::filesystem_error& e) {
                    cerr << "Error: " << e.what() << endl;
                }
            }
        explored_depth++;
        sort_childs_nested();
        max_depth++;
        num_child = num_child_dir + num_child_file + num_child_other;

        } else if (entry.is_regular_file(ec)) {
            type = Type::File;
            size = fs::file_size(p);
            max_depth = -1;
        } else {
            type = Type::Other;
            max_depth = -2;
        }
        if (entry.exists()) {
            const auto [_timestamp, _sctp] = get_last_write_time(entry);
            timestamp = _timestamp;
            sctp = _sctp;
        } else {
            timestamp = "N/A";
            string space(16, ' ');
            timestamp = timestamp + space;
        }
    }

    void load_recursive(int recurse_depth=1000) {
        error_code ec;
        fs::directory_entry entry(this->path, ec);
        if (ec) {
            cerr << "permission denied: " << entry.path() << endl;
        }
        if (entry.is_directory()) {
            childs_nested.clear();
            num_child_dir = 0;
            num_child_file = 0;
            num_child_other = 0;
            int explored_depth = 1;
            if (recurse_depth >= 0) {
                for (const auto& e : fs::directory_iterator(this->path, fs::directory_options::skip_permission_denied, ec)) {
                    if (ec) {
                        cerr << "permission denied: " << e.path() << endl;
                        ec.clear();
                        continue;
                    }
                    if (e.is_directory()) {
                        num_child_dir++;
                        childs_nested.emplace_back(e.path(), recurse_depth-explored_depth-1);
                    } else if (e.is_regular_file()) {
                        num_child_file++;
                        childs_nested.emplace_back(e.path(), -1);
                    } else {
                        num_child_other++;
                        childs_nested.emplace_back(e.path(), -1);
                    }
                }
            }
            sort_childs_nested();
            explored_depth++;
            num_child = num_child_dir + num_child_file + num_child_other;
        } else if (entry.is_regular_file(ec)) {
            cerr << "this is a file, not a directory." << endl;
        } else {
            cerr << "this is neither a directory or file." << endl;
        }
    }

    string allocate_typestr(const DirInfo& d, int num_indent, char indent_char) const {
        string typestr;
        if (num_indent<0) num_indent = 0;
        string indent(num_indent, indent_char);
        switch (d.type) {
            case Type::Directory: typestr = indent + "[D] "; break;
            case Type::File: typestr = indent + "[F] "; break;
            case Type::Other: typestr = indent + "[O] "; break;
        }
        return typestr;
    }

    string allocate_typestr(const ChildInfo& c, int num_indent, char indent_char) const {
        string typestr;
        if (num_indent<0) num_indent = 0;
        string indent(num_indent, indent_char);
        switch (c.type) {
            case ChildInfo::Type::Directory: typestr = indent + "[D] "; break;
            case ChildInfo::Type::File: typestr = indent + "[F] "; break;
            case ChildInfo::Type::Other: typestr = indent + "[O] "; break;
        }
        return typestr;
    }

    void print_childs(ostream& os, int disp_depth=10, int disp_num=20, int num_indent=4, string indent_mode="|-", char indent_char='-', char eliminator='|') const {
        string typestr;
        os << endl << endl << "root: " << this->path << endl << endl;
        os << "max_depth: " << this->max_depth << endl;
        os << "num_childs_recursive: " << this->num_childs_recursive << endl;
        os << "(num_childs_dir, num_child_file, num_child_other): " << "(" << this->num_childs_dir_recursive << ", " << this->num_childs_file_recursive << ", " << num_childs_other_recursive << ") " << endl << endl;
        typestr = allocate_typestr(*this, num_indent*0, indent_char);
        os << this->timestamp << " " << typestr << setw(6) << right << fixed << setprecision(1) << static_cast<double>(this->size)/1'000'000 << " [MB]    " << this->path << endl;
        int count = 0;
        int depth_printed = -1;
        string space(num_indent+1, ' ');
        for (ChildInfo c : this->childs) {
            if (count < disp_num) {
                if (c.depth <= disp_depth) {
                    if (indent_mode == "-") {
                        typestr = allocate_typestr(c, num_indent*(c.depth+1), indent_char);
                    } else if (indent_mode == "|-") {
                        string treeSpace = "";
                        string leafLine = allocate_typestr(c, num_indent, indent_char);
                        for (int i=0; i<c.depth; ++i) {
                            treeSpace += space;
                        }
                        typestr = treeSpace + '|' + leafLine;
                    }
                    os << c.timestamp << " " << typestr << setw(6) << right << fixed << setprecision(1) << static_cast<double>(c.size)/1'000'000 << " [MB]    " << c.path << endl;
                }
            count++;
            depth_printed = c.depth;
            } else {
                if (c.depth <= disp_depth) {
                    if (c.depth - depth_printed == 1) count = 0;
                } else {
                break;
                }
            }
        }
    }

    int print_childs_nested_all(ostream& os, const vector<DirInfo>& d_childs, int cur_depth, int disp_depth, int num_indent, string indent_mode, char indent_char, char eliminator) const {
        string typestr;
        if (cur_depth > disp_depth) return 0;
        size_t c = 0;
        string space(num_indent+1, ' ');
        for (const DirInfo& d : d_childs) {
            string leafLine = allocate_typestr(d, num_indent, indent_char);
            if (indent_mode == "-") {
                typestr = allocate_typestr(d, num_indent*(cur_depth+1), '-');
            } else if (indent_mode == "|-") {
                string treeSpace = "";
                for (int i=0; i<cur_depth; ++i) {
                    treeSpace += space;
                }
                typestr = treeSpace + '|' + leafLine;
            }
            os << d.timestamp << " " << typestr << setw(6) << right << fixed << setprecision(1) << static_cast<double>(d.size)/1'000'000 << " [MB]    " << d.path << endl;
            if (d.type == DirInfo::Type::Directory) {
                print_childs_nested_all(os, d.childs_nested, cur_depth+1, disp_depth, num_indent, indent_mode, indent_char, eliminator);
            }
            c++;
        }
        return 0;
    }

    void print_childs_nested(ostream& os, int disp_depth=10, int num_indent=4, string indent_mode="|-", char indent_char='-', char eliminator='|') const {
        string typestr;
        const DirInfo& d_now = *this;
        int cur_depth = 0;
        os << "path: " << d_now.path << endl << endl;
        os << "max_depth: " << d_now.max_depth << endl;
        os << "num_childs_recursive: " << d_now.num_childs_recursive << endl;
        os << "(num_childs_dir, num_child_file, num_child_other): " << "(" << d_now.num_childs_dir_recursive << ", " << d_now.num_childs_file_recursive << ", " << num_childs_other_recursive << ") " << endl << endl;
        typestr = allocate_typestr(d_now, num_indent*cur_depth, indent_char);
        os << d_now.timestamp << " " << typestr << setw(6) << right << fixed << setprecision(1) << static_cast<double>(d_now.size)/1'000'000 << " [MB]    " << d_now.path << endl;
        print_childs_nested_all(os, d_now.childs_nested, cur_depth, disp_depth, num_indent, indent_mode, indent_char, eliminator);
        cur_depth++;
    }

    void writeToFile(const string& filename) const {
        ofstream ofs(filename, ios::app);
        if (!ofs) {
            throw runtime_error("cannot open file: " + filename);
        }
        // print(ofs);
    }
};

bool can_read(const fs::path& p) {
    fs::file_status s = fs::status(p);
    auto perm = s.permissions();
    return (perm & fs::perms::owner_read) != fs::perms::none
        || (perm & fs::perms::group_read) != fs::perms::none
        || (perm & fs::perms::others_read) != fs::perms::none;
}


int main() {
    // main
    cout << endl << "main" << endl << "--------------------" << endl;
    cout << "ROOT: " << ROOT << endl;
    const fs::path HOME = fs::path(getenv("HOME"));
    cout << "HOME: " << HOME << endl;

    auto start = chrono::high_resolution_clock::now();


    // DirInfo dir = DirInfo(HOME);
    DirInfo dir = DirInfo(ROOT);
    // DirInfo dir = DirInfo(ROOT/"data"/"test");
    dir.print_childs(cout, 5, 4);

    // DirInfo dir = DirInfo(ROOT/"data"/"test", 4);
    // DirInfo dir = DirInfo(HOME, 100);
    // dir.load_recursive(1);
    // dir.print_childs_nested(cout, 0); //, 4, '-', ' ');


    auto end = chrono::high_resolution_clock::now();
    auto duration = duration_cast<chrono::milliseconds>(end-start).count();
    cout << "elapsed time: " << duration << " [ms]" << endl;

    // test

    // int num_file = 0;
    // int num_dir = 0;
    // int num_other = 0;
    // try {
        // for (const auto& entry: fs::directory_iterator(ROOT)) {
        // for (const auto& entry: fs::recursive_directory_iterator(ROOT)) {
            // FileInfo  f = FileInfo(entry.path());
            // f.print(cout);
            // string time_stamp = get_last_write_time(entry);
            // if (entry.is_directory()) {
            //     cout << "[DIR]   " << time_stamp << " " << static_cast<double>(get_dir_size(entry))/1'000'000 << "[MB] " << entry.path().string() << fixed << setprecision(2) << endl;
            //     num_dir += 1;
            // } else if (entry.is_regular_file()) {
            //     cout << "[FILE]  " << time_stamp << " " << static_cast<double>(entry.file_size())/1'000'000 << "[MB] " << entry.path().string() << fixed << setprecision(2) << endl;
            //     num_file += 1;
            // } else {
            //     cout << "[OTHER] " << time_stamp << " " << entry.path().string() << " " << endl;
            //     num_other += 1;
            // }
    //     }
    // } catch (const fs::filesystem_error& e) {
    //     cerr << "Error: " << e.what() << endl;
    // }
    // cout << "dirs: " << num_dir << endl << "files: " << num_file << endl << "others: " << num_other << endl;

    // FileReader file(target_file);
    // string content = file.readAll();
    // cout << "result: " << endl << content << endl;

    // LineReader line(target_file);
    // line.processLineByLine([](const string& line) {
    //     cout << line << endl;
    // });
    // cout << endl;
    // vector<string> match;
    // match = line.findLines("hello");
    // for (string s: match) {
    //     cout << s << endl;
    // }

    // BinaryReader<Record> binary_file(target_file);
    // Record content_bi = binary_file.readFixed();
    // cout << "id: " << content_bi.id << endl;
    // cout << "value: " << content_bi.value << endl;
    // cout << "content: " << endl << content_bi.name << endl;

    // MemoryMappedFile mapped(target_file.string());
    // cout << mapped.getData() << endl;
    // cout << mapped.getSize() << endl;
    // MemoryMappedFile* mapped = new MemoryMappedFile(target_file.string());
    // cout << mapped->getData() << endl;
    // cout << mapped->getSize() << endl;
    // delete mapped;

    // BufferedReader buffered(target_file.string(), 1000);
    // vector<char> buf(1001);
    // size_t size = buffered.read(buf.data(), 10);
    // vector<char> buf2(1001);
    // size_t size2 = buffered.read(buf2.data(), 100);
    // cout << endl << "----------" << endl;
    // cout << endl << "data: (" << size << ")" << endl << buf.data() << endl;
    // cout << endl << "data: (" << size2 << ")" << endl << buf2.data() << endl;
    // cout << endl << "----------" << endl;

    // cout << endl << "test" << endl << "--------------------" << endl << endl;

    // lambda function test
    // auto f = [](int x) {
    //     return x * 2;
    // };
    // cout << f(5) << endl;
    // int a = 10;
    // auto f2 = [a](int x) {
    //     return x + a;
    // };
    // cout << f2(5) << endl;

    // vector test
    // vector<int> nums;
    // nums.push_back(10);
    // nums.push_back(20);
    // vector<string> names = {"A", "B", "C"};
    // names.push_back("F");
    // names.push_back("TEST");
    // cout << "size of nums: " << nums.size() << endl;
    // cout << nums[0] << endl;
    // cout << nums.at(1) << endl;
    // for (int i=0; i<nums.size(); ++i) {
    //     cout << nums[i] << endl;
    // }
    // for (string x : names) {
    //     cout << x << endl;
    // }

    // memory map test
    // int fd = open(target_file.string().c_str(), O_RDONLY);
    // cout << "fd: " << fd << endl;
    // struct stat sb;
    // fstat(fd, &sb);
    // cout << "sb: " << typeid(sb).name() << endl;
    // cout << "size: " << sb.st_size << endl;
    // cout << "mode: " << oct << sb.st_mode << dec << endl;
    // cout << "time stamp: " << asctime(localtime(&sb.st_mtime)) << endl;
    // void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    // if (mapped == MAP_FAILED) {
    //     throw runtime_error("failed to map memory");
    // }
    // char* data = static_cast<char*>(mapped);
    // ofstream log(ROOT/"dump.txt");
    // for (size_t i=0; i<sb.st_size; ++i) {
    //     unsigned char ch = data[i];
    //     if (ch == '\n') {ch = '*'; }
    //     cout << i << ": ";
    //     cout << "(" << ch << "), ";// << endl;
    //     if (i%10 == 0) {cout << endl;}
    //     log << i << ": ";
    //     log << "(" << ch << ")" << endl;
    // }
    // log.close();
    // munmap(mapped, sb.st_size);
    // close(fd);

    // adress and pointer
    // int a = 3;
    // int* p = &a;
    // int& x = a;
    // printf("a: %d\n", a);
    // printf("p: %d\n", *p);
    // printf("x: %d\n", x);
    // printf("a: %p\n", &a);
    // printf("p: %p\n", p);
    // printf("x: %p\n", &x);

    // filesystem time test
    // fs::path target = fs::path(ROOT/"data"/"test"/"eximg3.jpg");
    // error_code ec;
    // fs::directory_entry entry(target, ec);
    // chrono::time_point ftime = entry.last_write_time();
    // chrono::time_point sctp = chrono::clock_cast<chrono::system_clock>(ftime);
    // time_t cftime = chrono::system_clock::to_time_t(sctp);
    // tm* tm = localtime(&cftime);
    // cout << "time_point ftime: " << ftime << endl; // file time
    // cout << "time_point sctp: " << sctp << endl; // syste cloc time point
    // cout << "time_t cftime: " << cftime << endl; // C time
    // cout << "tm* tm: " << tm << endl; // struct tm
    // cout << "tm* tm: " << tm->tm_mon << endl;

    cout << endl << endl << "--------------------" << endl << "complete" << endl;
    return 0;
}
