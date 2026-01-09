#include <iostream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>



char fullpath[512];

std::pair<std::string, std::string> parse_command(const std::string &input) {
    std::string cmd, rest;
    int i = 0;

    while (i < input.size() && input[i] != ' ')
        cmd += input[i++];

    if (i < input.size())
        rest = input.substr(i + 1);

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
                std::strcpy(out, fullpath);
                return true;
            }

            if (path[i] == '\0')
                break;
        } else {
            p[j++] = path[i];
        }
    }
    return false;
}

void build_argv(const std::string &cmd, const std::string &args,char *argv[]) {
    int idx = 0;

    argv[idx] = new char[cmd.size() + 1];
    std::strcpy(argv[idx++], cmd.c_str());

    char buf[256];
    int j = 0;

    for (int i = 0; i <= args.size(); i++) {
        if (i == args.size() || args[i] == ' ') {
            if (j > 0) {
                buf[j] = '\0';
                argv[idx] = new char[j + 1];
                std::strcpy(argv[idx++], buf);
                j = 0;
            }
        } else {
            buf[j++] = args[i];
        }
    }

    argv[idx] = nullptr;
}

bool execute_external(const std::string &exe, const std::string &args) {
    char pathbuf[512];

    if (!find_in_path(exe, pathbuf))
        return false;

    pid_t pid = fork();

    if (pid == 0) {
        char *argv[64];
        build_argv(exe, args, argv);
        execve(pathbuf, argv, NULL);
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
        if (!std::getline(std::cin, input))
            break;

        auto parsed = parse_command(input);

        if (parsed.first == "exit")
            break;

        if (parsed.first == "echo") {
            std::cout << parsed.second << '\n';
            continue;
        }
	if(parsed.first =="pwd"){
		char result[512];
		getcwd(result,512);
		std::cout<<result<<'\n';
		continue;
	}	
	if(parsed.first =="cd"){
		if(parsed.second[0]=='~')
		{
			const char *path = getenv("HOME");
			chdir(path);
			continue;
		}	
		int res=chdir(parsed.second.c_str());
		if(res==-1)
		{
			std::cout<<"cd: "<<parsed.second<<": No such file or directory"<<'\n';
		}
		continue;
	
	}
        if (parsed.first == "type") {
            if (parsed.second == "echo" ||
                parsed.second == "exit" ||
                parsed.second == "type" ||
                parsed.second == "pwd" ||
                parsed.second == "cd") {
                std::cout << parsed.second << " is a shell builtin\n";
                continue;
            }

            char result[512];
            if (find_in_path(parsed.second, result)) {
                std::cout << parsed.second << " is " << result << '\n';
            } else {
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

