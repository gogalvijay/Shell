#include <iostream>
#include <string>
#include<dirent.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>


std::pair<std::string,std::string> parse_command(const std::string& input) {
    std::string command;
    std::string remaining_command;
    for (int i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (ch == ' ') {
            remaining_command = input.substr(i + 1);
            break;
        }
        command += ch;
    }
    return std::make_pair(command, remaining_command);
}

bool check_file_present(const char *path, std::string executable)
{
    DIR *dir = opendir(path);
    if (!dir)
        return false;

    struct dirent *entry;
    struct stat sb;
    char fullpath[512];

    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (strcmp(entry->d_name, executable.c_str()) == 0)
        {
            snprintf(fullpath, sizeof(fullpath),
                     "%s/%s", path, entry->d_name);

            if (stat(fullpath, &sb) == 0 &&
                S_ISREG(sb.st_mode) &&
                access(fullpath, X_OK) == 0)
            {
                closedir(dir);
                return true;
            }
        }
    }

    closedir(dir);
    return false;
}


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

 
  while(1){
    std::cout << "$ ";
    
    std::string command;
    std::getline(std::cin,command);
    
    std::pair<std::string,std::string> parsed_command = parse_command(command);
    
    if(parsed_command.first=="exit"){
          break;
    }
    else if(parsed_command.first == "echo"){
         std::cout << parsed_command.second << '\n';
    }
    
    else if(parsed_command.first == "type"){
         if(parsed_command.second=="echo" || parsed_command.second=="type" || parsed_command.second=="exit"){
             std::cout << parsed_command.second << " is a shell builtin"<<'\n';
             continue;
         }
         
         const char *path = getenv("PATH");
         char p[256];
         p[0]='\0';
         bool is_present=false;
         int j=0;
         //std::cout<<path<<'\n';
         for(int i=0;path[i]!='\0';i++)
         {
         	char ch=path[i];
         	if(ch==':')
         	{
         		p[j]='\0';
         		//std::cout<<p<<' ';
         		if(check_file_present(p,parsed_command.second))
         		{
         			is_present=true;
         			std::cout<<parsed_command.second << " is "<<p<<"/"<<parsed_command.second<<'\n';
         			break;
         		}
         		p[0]='\0';
         		j=0;
         	}
         	else 
         		p[j++]=ch;
         }
         
          
         if(!is_present){
             std::cout << parsed_command.second << ": not found"<<'\n';
         }
    }
    
    else 
        std::cout <<command<< ": command not found"<<'\n';
  }
}
