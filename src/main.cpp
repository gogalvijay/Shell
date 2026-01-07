#include <iostream>
#include <string>



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
         }
         else{
             std::cout << parsed_command.second << ": not found"<<'\n';
         }
    }
    
    else 
        std::cout <<command<< ": command not found"<<'\n';
  }
}
