#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>


static bool is_all_spaces(const std::string& s) {
    for (char c : s) if (!std::isspace((unsigned char)c)) return false;
    return true;
}


static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string cur;

    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        if (in_double) {
            if (c == '\\' && i + 1 < line.size()) {
                char nxt = line[i + 1];

                if (nxt == '"' || nxt == '\\') {
                    cur.push_back(nxt);
                    i++;
                    continue;
                }
            }
            if (c == '"') {
                in_double = false; 
            } else {
                cur.push_back(c);
            }
            continue;
        }

        if (in_single) {
            if (c == '\'') {
                in_single = false; 
            } else {
                cur.push_back(c);
            }
            continue;
        }

        if (std::isspace((unsigned char)c)) {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
            continue;
        }

        if (c == '"') { in_double = true; continue; }
        if (c == '\'') { in_single = true; continue; }

        cur.push_back(c);
    }

    if (!cur.empty()) tokens.push_back(cur);

    return tokens;
}

static std::string expand_env_in_token(const std::string& tok) {
    std::string out;
    for (size_t i = 0; i < tok.size(); ) {
        if (tok[i] == '$') {
            size_t j = i + 1;
            if (j < tok.size() && (std::isalpha((unsigned char)tok[j]) || tok[j] == '_')) {
                j++;
                while (j < tok.size() && (std::isalnum((unsigned char)tok[j]) || tok[j] == '_')) j++;
                std::string var = tok.substr(i + 1, j - (i + 1));
                const char* val = std::getenv(var.c_str());
                if (val) out += val; 
                i = j;
            } else {
                out.push_back('$');
                i++;
            }
        } else {
            out.push_back(tok[i]);
            i++;
        }
    }
    return out;
}

static void expand_env(std::vector<std::string>& tokens) {
    for (auto& t : tokens) 
      t = expand_env_in_token(t);
}

struct Command {
    std::vector<std::string> argv;
    std::string in_file;
    std::string out_file;
    bool append = false;
};

static bool parse_command(const std::vector<std::string>& tokens, Command& cmd, std::string& err) {
    cmd = Command{};
    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& t = tokens[i];

        if (t == "<" || t == ">" || t == ">>") {
            if (i + 1 >= tokens.size()) {
                err = "Syntax error: missing file after redirection " + t;
                return false;
            }
            const std::string& file = tokens[i + 1];
            if (t == "<") cmd.in_file = file;
            else {
                cmd.out_file = file;
                cmd.append = (t == ">>");
            }
            i++; 
        } else {
            cmd.argv.push_back(t);
        }
    }

    if (cmd.argv.empty()) {
        err = "Empty command";
        return false;
    }
    return true;
}

static std::string make_prompt() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) {
        return std::string("myshell:") + buf + "$ ";
    }
    return "myshell$ ";
}


static void print_help() {
    std::cout
        << "Builtins:\n"
        << "  cd [path]    Change directory (cd without args goes to $HOME)\n"
        << "  pwd          Print current directory\n"
        << "  history      Show command history (in-memory)\n"
        << "  help         Show this help\n"
        << "  exit         Exit shell\n"
        << "\nRedirections:\n"
        << "  cmd > out.txt     overwrite output\n"
        << "  cmd >> out.txt    append output\n"
        << "  cmd < in.txt      read input from file\n";
}

static int builtin_cd(const std::vector<std::string>& argv) {
    const char* path = nullptr;
    if (argv.size() < 2) {
        path = std::getenv("HOME");
        if (!path) path = "/";
    } else {
        path = argv[1].c_str();
    }
    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

static int builtin_pwd() {
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf))) {
        perror("pwd");
        return 1;
    }
    std::cout << buf << "\n";
    return 0;
}

static bool is_builtin(const std::string& name) {
    return name == "cd" || name == "pwd" || name == "exit" || name == "help" || name == "history";
}


static int run_external(const Command& cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {

        if (!cmd.in_file.empty()) {
            int fd = open(cmd.in_file.c_str(), O_RDONLY);
            if (fd < 0) { perror("open <"); _exit(127); }
            if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 <"); _exit(127); }
            close(fd);
        }

        if (!cmd.out_file.empty()) {
            int flags = O_WRONLY | O_CREAT;
            flags |= cmd.append ? O_APPEND : O_TRUNC;
            int fd = open(cmd.out_file.c_str(), flags, 0644);
            if (fd < 0) { perror("open >"); _exit(127); }
            if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 >"); _exit(127); }
            close(fd);
        }


        std::vector<char*> argv;
        argv.reserve(cmd.argv.size() + 1);
        for (const auto& s : cmd.argv) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        perror("execvp");
        _exit(127);
    }


    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}


int main() {
    std::vector<std::string> history;
    history.reserve(200);

    while (true) {
        std::cout << make_prompt();
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break; 
        }
        if (is_all_spaces(line)) continue;


        auto tokens = tokenize(line);

   
        expand_env(tokens);

        
        Command cmd;
        std::string err;
        if (!parse_command(tokens, cmd, err)) {
            
            if (err != "Empty command") std::cerr << err << "\n";
            continue;
        }

        
        history.push_back(line);
        if (history.size() > 200) history.erase(history.begin());

        
        const std::string& name = cmd.argv[0];

        if (name == "exit") {
            break;
        } else if (name == "cd") {
            builtin_cd(cmd.argv);
            continue;
        } else if (name == "pwd") {
            builtin_pwd();
            continue;
        } else if (name == "help") {
            print_help();
            continue;
        } else if (name == "history") {
            for (size_t i = 0; i < history.size(); i++) {
                std::cout << (i + 1) << "  " << history[i] << "\n";
            }
            continue;
        }

     
        run_external(cmd);
    }

    return 0;
}

