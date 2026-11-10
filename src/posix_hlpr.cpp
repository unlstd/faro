#include "posix_hlpr.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <vector>

// ---------------------------------------------------------------------
// The following functions are helpers for executing command using posix
// ---------------------------------------------------------------------

int faro::posix::exec_command(const std::vector<std::string> &args, const std::string &working_directory) {
  pid_t pid = fork(); // we create new child process

  if (pid == 0) { // if pid is 0 it means it's child whose executing this script 
    chdir(working_directory.c_str());

    // execvp doesn't accept vector<std::string> so we create vector<char*>
    std::vector<char*> argv; 

    for (const auto& arg : args)
      argv.push_back(const_cast<char*>(arg.c_str()));

    argv.push_back(nullptr); // execvp wants the list to be ended with nullptr

    // execvp replaces current process with command process so it kills the child faro instance
    execvp(argv[0], argv.data());
    _exit(127); // we return an error if the process wasn't killed by previous command
  }

  if (pid < 0) // if the pid is -1 or lower it's an error
    return -1;

  // if pid is greater than 0 it's still parent executing the code in this case we do nothing
  // as the child's process was already killed in the previous section the following
  // code will be executed only as a faro
  // REMEMBER this code is running pararell to the child's code
  //  so the next code will be executed before or at once with the child's one.
  int status;
  waitpid(pid, &status, 0); // we wait for the exec code from child's pid

  if (WIFEXITED(status)) // checks if the process ended normally
    return WEXITSTATUS(status); // if so we return the status code
  
  return -1;
}
