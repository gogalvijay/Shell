#include <iostream>
#include <string>
#include <vector>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h> 

char fullpath[512];

// --- Terminal Raw Mode Handling ---
struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    // Disable ICANON (line buffering) and ECHO (automatic printing)
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
// ----------------------------------

struct Redirect {
    bool out_enabled = false;
    bool err_enabled = false;
    bool out_append = false;
    bool err_append = false;
    std::string out_file;
    std::string err_file;
};

Redirect parse_redirect(std::string &args) {
    Redirect r;
    size_t pos;

    if ((pos = args.find("2>>")) != std::string::npos) {
        r.err_enabled = true;
        r.err_append = true;
        size_t start = pos + 3;
        while (start < args.size() && args[start] == ' ') start++;
        r.err_file = args.substr(start);
        args = args.substr(0, pos);
    } else if ((pos = args.find("2>")) != std::string::npos) {
        r.err_enabled = true;
        size_t start = pos + 2;
        while (start < args.size() && args[start] == ' ') start++;
        r.err_file = args.substr(start);
        args = args.substr(0, pos);
    }

    if ((pos = args.find("1>>")) != std::string::npos ||
        (pos = args.find(">>")) != std::string::npos) {
        r.out_enabled = true;
        r.out_append = true;
        size_t skip = (args[pos] == '1') ? 3 : 2;
        size_t start = pos + skip;
        while (start < args.size() && args[start] == ' ') start++;
        r.out_file = args.substr(start);
        args = args.substr(0, pos);
    } else if ((pos = args.find("1>")) != std::string::npos ||
               (pos = args.find(">")) != std::string::npos) {
        r.out_enabled = true;
        size_t skip = (args[pos] == '1') ? 2 : 1;
        size_t start = pos + skip;
        while (start < args.size() && args[start] == ' ') start++;
        r.out_file = args.substr(start);
        args = args.substr(0, pos);
    }

    return r;
}

std::pair<std::string, std::string> parse_command(const std::string &input) {
    std::string cmd, rest;
    size_t i = 0;
    while (i < input.size() && input[i] == ' ') i++;

    bool single = false, dbl = false;

    while (i < input.size()) {
        char c = input[i];
        if (c == ' ' && !single && !dbl) break;
        if (c == '\'' && !dbl) { single = !single; i++; continue; }
        if (c == '"' && !single) { dbl = !dbl; i++; continue; }
        cmd += c;
        i++;
    }

    if (i < input.size() && input[i] == ' ') i++;
    if (i < input.size()) rest = input.substr(i);
    return {cmd, rest};
}

bool check_file_present(const char *path, const std::string &exe) {
    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *entry;
    struct stat sb;

    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, exe.c_str()) == 0) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
            if (stat(fullpath, &sb) == 0 &&
                S_ISREG(sb.st_mode) &&
                access(fullpath, X_OK) == 0) {
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);
    return false;
}

bool find_in_path(const std::string &exe, char *out) {
    const char *path = getenv("PATH");
    char p[256];
    int j = 0;

    for (int i = 0;; i++) {
        if (path[i] == ':' || path[i] == '\0') {
            p[j] = '\0';
            j = 0;
            if (check_file_present(p, exe)) {
                strcpy(out, fullpath);
                return true;
            }
            if (path[i] == '\0') break;
        } else {
            p[j++] = path[i];
        }
    }
    return false;
}

void build_argv(const std::string &cmd, const std::string &args, char *argv[]) {
    int idx = 0;
    argv[idx++] = strdup(cmd.c_str());

    std::string token;
    bool single = false, dbl = false;

    for (size_t i = 0; i < args.size(); i++) {
        char c = args[i];

        if (c == '\\') {
            if (single) token += c;
            else if (dbl) {
                if (i + 1 < args.size()) {
                    char n = args[i + 1];
                    if (n == '"' || n == '\\' || n == '$' || n == '\n') {
                        token += n;
                        i++;
                    } else token += c;
                }
            } else {
                if (i + 1 < args.size()) token += args[++i];
            }
            continue;
        }

        if (c == '\'' && !dbl) { single = !single; continue; }
        if (c == '"' && !single) { dbl = !dbl; continue; }

        if (c == ' ' && !single && !dbl) {
            if (!token.empty()) {
                argv[idx++] = strdup(token.c_str());
                token.clear();
            }
            continue;
        }

        token += c;
    }

    if (!token.empty()) argv[idx++] = strdup(token.c_str());
    argv[idx] = nullptr;
}

bool execute_external(const std::string &exe, std::string args) {
    char pathbuf[512];
    if (!find_in_path(exe, pathbuf)) return false;

    Redirect r = parse_redirect(args);
    pid_t pid = fork();

    if (pid == 0) {
        if (r.out_enabled) {
            int flags = O_WRONLY | O_CREAT | (r.out_append ? O_APPEND : O_TRUNC);
            int fd = open(r.out_file.c_str(), flags, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (r.err_enabled) {
            int flags = O_WRONLY | O_CREAT | (r.err_append ? O_APPEND : O_TRUNC);
            int fd = open(r.err_file.c_str(), flags, 0644);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        char *argv[64];
        build_argv(exe, args, argv);
        execve(pathbuf, argv, nullptr);
        perror("execve");
        _exit(1);
    } else {
        waitpid(pid, nullptr, 0);
    }
    return true;
}


std::string read_input_with_autocomplete() {
    std::string input;
    char c;
    std::vector<std::string> builtins = {"echo", "exit"};

    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n') {
            std::cout << '\n';
            break;
        } else if (c == '\t') {
            
            int matches = 0;
            std::string match;
            
            for (const auto &cmd : builtins) {
                if (cmd.rfind(input, 0) == 0) { 
                    matches++;
                    match = cmd;
                }
            }
            
            if (matches == 1) {
                std::string remaining = match.substr(input.length());
                remaining += " ";
                
                std::cout << remaining;
                input += remaining;
            } else {
                std::cout << '\a';
            }
        } else if (c == 127) { 
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
            }
        } else {
            std::cout << c;
            input += c;
        }
    }
    return input;
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    
    enableRawMode();

    while (true) {
        std::cout << "$ ";
        
       
        std::string input = read_input_with_autocomplete();
        
        if (input.empty()) continue;

        auto parsed = parse_command(input);
        if (parsed.first == "exit") break;

        if (parsed.first == "echo") {
            Redirect r = parse_redirect(parsed.second);

            int so = -1, se = -1, fo = -1, fe = -1;

            if (r.out_enabled) {
                so = dup(STDOUT_FILENO);
                int flags = O_WRONLY | O_CREAT | (r.out_append ? O_APPEND : O_TRUNC);
                fo = open(r.out_file.c_str(), flags, 0644);
                dup2(fo, STDOUT_FILENO);
            }
            if (r.err_enabled) {
                se = dup(STDERR_FILENO);
                int flags = O_WRONLY | O_CREAT | (r.err_append ? O_APPEND : O_TRUNC);
                fe = open(r.err_file.c_str(), flags, 0644);
                dup2(fe, STDERR_FILENO);
            }

            bool single = false, dbl = false, last = false;

            for (size_t i = 0; i < parsed.second.size(); i++) {
                char c = parsed.second[i];
                if (c == '\'' && !dbl) { single = !single; continue; }
                if (c == '"' && !single) { dbl = !dbl; continue; }
                if (c == '\\' && !single && i + 1 < parsed.second.size()) {
                    std::cout << parsed.second[++i];
                    last = false;
                    continue;
                }
                if (c == ' ' && !single && !dbl) {
                    if (!last) std::cout << ' ';
                    last = true;
                    continue;
                }
                last = false;
                std::cout << c;
            }

            std::cout << '\n';

            if (r.out_enabled) { dup2(so, STDOUT_FILENO); close(so); close(fo); }
            if (r.err_enabled) { dup2(se, STDERR_FILENO); close(se); close(fe); }
            continue;
        }

        if (parsed.first == "pwd") {
            char buf[512];
            getcwd(buf, sizeof(buf));
            std::cout << buf << '\n';
            continue;
        }

        if (parsed.first == "cd") {
            const char *p = parsed.second == "~" ? getenv("HOME") : parsed.second.c_str();
            if (chdir(p) == -1)
                std::cout << "cd: " << parsed.second << ": No such file or directory\n";
            continue;
        }

        if (parsed.first == "type") {
            if (parsed.second == "echo" || parsed.second == "exit" ||
                parsed.second == "type" || parsed.second == "pwd" ||
                parsed.second == "cd") {
                std::cout << parsed.second << " is a shell builtin\n";
            } else {
                char p[512];
                if (find_in_path(parsed.second, p))
                    std::cout << parsed.second << " is " << p << '\n';
                else
                    std::cout << parsed.second << ": not found\n";
            }
            continue;
        }

        if (!execute_external(parsed.first, parsed.second))
            std::cout << parsed.first << ": command not found\n";
    }
    return 0;
}
