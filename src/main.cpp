#include <iostream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

char fullpath[512];

struct Redirect {
    bool enabled = false;
    std::string file;
};

Redirect parse_redirect(std::string &args) {
    Redirect r;
    size_t pos;

    if ((pos = args.find("1>")) != std::string::npos ||
        (pos = args.find(">"))  != std::string::npos) {

        r.enabled = true;
        size_t skip = (args[pos] == '1') ? 2 : 1;
        size_t start = pos + skip;

        while (start < args.size() && args[start] == ' ')
            start++;

        r.file = args.substr(start);
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


void build_argv(const std::string &cmd,
                const std::string &args,
                char *argv[]) {
    int idx = 0;
    argv[idx++] = strdup(cmd.c_str());

    std::string token;
    bool single = false, dbl = false;

    for (size_t i = 0; i < args.size(); i++) {
        char c = args[i];

      
        if (c == '\\') {
            if (single) {
                
                token += c;
            } else if (dbl) {
               
                if (i + 1 < args.size()) {
                    char next = args[i + 1];
                    if (next == '"' || next == '\\' || next == '$' || next == '\n') {
                        token += next;
                        i++;
                    } else {
                        token += c;
                    }
                }
            } else {
                
                if (i + 1 < args.size()) {
                    token += args[++i];
                }
            }
            continue;
        }

      
        if (c == '\'' && !dbl) {
            single = !single;
            continue;
        }

        if (c == '"' && !single) {
            dbl = !dbl;
            continue;
        }

      
        if (c == ' ' && !single && !dbl) {
            if (!token.empty()) {
                argv[idx++] = strdup(token.c_str());
                token.clear();
            }
            continue;
        }

        token += c;
    }

    if (!token.empty())
        argv[idx++] = strdup(token.c_str());

    argv[idx] = nullptr;
}


bool execute_external(const std::string &exe, std::string args) {
    char pathbuf[512];
    if (!find_in_path(exe, pathbuf)) return false;

    Redirect r = parse_redirect(args);
    pid_t pid = fork();

    if (pid == 0) {
        if (r.enabled) {
            int fd = open(r.file.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC,
                          0644);
            if (fd < 0) { perror("open"); _exit(1); }
            dup2(fd, STDOUT_FILENO);
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



int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    while (true) {
        std::cout << "$ ";
        std::string input;
        if (!std::getline(std::cin, input)) break;

        auto parsed = parse_command(input);

        if (parsed.first == "exit") break;

     

       if (parsed.first == "echo") {
    Redirect r = parse_redirect(parsed.second);

    int saved_stdout = -1;
    int fd = -1;

    if (r.enabled) {
        saved_stdout = dup(STDOUT_FILENO);
        fd = open(r.file.c_str(),
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);
        dup2(fd, STDOUT_FILENO);
    }

    bool single = false, dbl = false;
    bool last_space = false;

    for (size_t i = 0; i < parsed.second.size(); i++) {
        char c = parsed.second[i];

        if (c == '\'' && !dbl) {
            single = !single;
            continue;
        }

        if (c == '"' && !single) {
            dbl = !dbl;
            continue;
        }

        if (c == '\\' && !single && i + 1 < parsed.second.size()) {
            std::cout << parsed.second[++i];
            last_space = false;
            continue;
        }

        if (c == ' ' && !single && !dbl) {
            if (!last_space)
                std::cout << ' ';
            last_space = true;
            continue;
        }

        last_space = false;
        std::cout << c;
    }

    std::cout << '\n';

    if (r.enabled) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd);
    }
    continue;
}


 

        if (parsed.first == "pwd") {
            char buf[512];
            getcwd(buf, sizeof(buf));
            std::cout << buf << '\n';
            continue;
        }

        if (parsed.first == "cd") {
            const char *path =
                parsed.second == "~" ? getenv("HOME") : parsed.second.c_str();
            if (chdir(path) == -1)
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

       

        if (!execute_external(parsed.first, parsed.second)) {
            std::cout << parsed.first << ": command not found\n";
        }
    }
    return 0;
}

