#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
//#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <cstring>

#include <sys/wait.h>
#include <sys/types.h>
#include "fcntl.h"
#include <sched.h>
#include <thread>
#include <sys/stat.h>


#include <sys/sysinfo.h>
#include <sched.h>


using namespace std;
#define MAX_ARGS_IN_CMD 20
#define MAX_COMMAND_LENGTH 80
//JobsList::JobEntry * SmallShell::current_jobs;
//vector<JobsList::JobEntry *> JobsList::jobs;
//vector<JobsList::JobEntry *> JobsList::times;
//SmallShell small;


const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif




string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundCommand(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char* cmd_line)
{
    this->arguments = new char *[MAX_ARGS_IN_CMD];
    this->cmd_line=new char[MAX_COMMAND_LENGTH];
    this->cmd_line= strcpy(this->cmd_line,cmd_line);
    this->args_size = _parseCommandLine(this->cmd_line,arguments);
}

Command::~Command() {
    if (cmd_line != nullptr) {
        delete[] cmd_line;
    }

    // Free memory allocated for each argument in arguments
    if (arguments != nullptr) {
        for (int i = 0; i < args_size; i++) {
            if (arguments[i] != nullptr) {
                delete[] arguments[i];
            }
        }

        // Free memory allocated for arguments
        delete[] arguments;
    }
}




SmallShell::SmallShell():   prompt("smash> "),jobs_list(), current_fg_pid(-1), cmd_running("")   {
    smash_pid= getpid();

    if (smash_pid == -1) // when pid didnt work
    {
        perror("smash error: getpid failed");
        return;
    }
}



BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {};



/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    // For example:

    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if((cmd_s).find(">") != std::string::npos)
    {
        return new RedirectionCommand(cmd_line);
    }

    if((cmd_s).find("|") != std::string::npos)
    {
        return new PipeCommand(cmd_line);
    }
    if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("chprompt") == 0) {
        return new PromptCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line);
    }
    else if (firstWord.compare("jobs") == 0)
    {
        return new JobsCommand(cmd_line, &(this->jobs_list));
    }
    else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(cmd_line);
    }

    else if (firstWord.compare("fg") == 0)
    {
        return new ForegroundCommand(cmd_line, &(this->jobs_list));
    }
    else if (firstWord.compare("bg") == 0)
    {
        return new BackgroundCommand(cmd_line, &(this->jobs_list));
    }
    else if (firstWord.compare("kill") == 0)
    {
        return new KillCommand(cmd_line);
    }
    else if (firstWord.compare("setcore") == 0)
    {
        return new SetcoreCommand(cmd_line);
    }
    else if(firstWord.compare("chmod") == 0){
        return new ChmodCommand(cmd_line);
    }
    else if(firstWord.compare("getfiletype") == 0){
        return new GetFileTypeCommand(cmd_line);
    }
    else {
        return new ExternalCommand(cmd_line);
    }
//  else if ...
//  .....
//  else {
//    return new ExternalCommand(cmd_line);
//  }

    return nullptr;
}

bool isDigitsOnly(string str) {
    for (char c : str) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return true;
}

void convertCmdLineToArgs(string cmd_line, vector<string>* arguments){
    const char* command = cmd_line.c_str();
    char** args_from_cmd = new char*[MAX_ARGS_IN_CMD]; //max of 20 args
    for(int i=0;i<MAX_ARGS_IN_CMD;++i){
        args_from_cmd[i] = new char();
        args_from_cmd[i] = nullptr;
    }
    _parseCommandLine(command,args_from_cmd);
    for(int i=0;i<MAX_ARGS_IN_CMD;++i){
        if(args_from_cmd[i]!= nullptr){
            arguments->push_back(args_from_cmd[i]);
        }
    }
    delete[] args_from_cmd;
}


void SmallShell::executeCommand(const char *cmd_line) {
    SmallShell& smash = SmallShell::getInstance();
    smash.cmd_running = string(cmd_line);
    jobs_list.removeFinishedJobs();
    ///JobsList::removeFinishedTimes();
    Command *cmd = CreateCommand(cmd_line);
    if (cmd == nullptr) {
        delete cmd;
    }
    if (_isBackgroundCommand(cmd_line))
    {
        smash.current_fg_pid = -1;
    }
//    else
//    {
//        smash.current_fg_pid = 1; //TODO: check if this is the right value
//
//    }
    cmd->execute();
}



PromptCommand::PromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void PromptCommand::execute() {
    string updated_name = "smash";
    if (this->args_size > 1) {
        updated_name = this->arguments[1];
    }
    updated_name.append("> ");
    SmallShell & shell = SmallShell::getInstance();
    shell.prompt = updated_name;
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};

void ShowPidCommand::execute() {
    SmallShell & shell = SmallShell::getInstance();
    if(shell.smash_pid >= 0)
    {
        cout << "smash pid is " << shell.smash_pid << endl;
    }
    else
    {
        perror("smash error: getpid failed");
    }
    return;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
void GetCurrDirCommand::execute() {
    char path[MAX_COMMAND_LENGTH];
    getcwd(path, sizeof(path));
    cout << path << endl;
    return;
}



ChangeDirCommand::ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}

void ChangeDirCommand::execute()
{
    int result = 0;
    char current_dir[MAX_COMMAND_LENGTH];
    getcwd(current_dir, sizeof(current_dir));
    if (this->args_size > 2)
    {
        cerr <<"smash error: cd: too many arguments" << endl;
    }
    else
    {
        if(this->args_size == 2)
        {
            if (strcmp(this->arguments[1], "-") == 0) {
                if (ChangeDirCommand::path_history.empty()) { //can't find previous directory
                    cerr <<"smash error: cd: OLDPWD not set" << endl;
                }
                else
                {
                    result = chdir(ChangeDirCommand::path_history.back().c_str()); //prev directory is in the end of list
                    if (result == 0)
                    {
                        ChangeDirCommand::path_history.pop_back(); //now prev dir is update
                        ChangeDirCommand::path_history.push_back(current_dir);

                    }
                }
            }
            else {
                result = chdir(this->arguments[1]); // new directory is
                if (result != 0) {
                    // you have an error :(
                    cerr <<"smash error: chdir failed: No such file or directory" << endl;
                }
                else{
                    ChangeDirCommand::path_history.push_back(current_dir);
                }
            }
        }
    }
}

JobsList::JobEntry::JobEntry(int j_id, int j_process_id, Command* j_command, bool is_stopped, time_t time_job):
        j_id(j_id), j_process_id(j_process_id), j_command(j_command),
        is_stopped(is_stopped), time_job(time_job) {}

JobsList::JobsList() : jobs_list({}), counter(0){}


//JobsList::JobEntry::~JobEntry() {
//    // Free memory allocated for j_command
//    delete j_command;
//}


JobsList::~JobsList() {
    // Free memory allocated for each JobEntry in jobs_list
    for (auto& job_entry : jobs_list) {
        delete job_entry.j_command;
    }

}




int JobsList::getHighestJob()
{
    int max = 0;
    auto it = this->jobs_list.begin();
    while (it != this->jobs_list.end()) {
        if(it->j_id > max)
        {
            max = it->j_id;
        }
        it++;
    }
    return max;
}

void JobsList::addJob(Command *cmd, int pid, bool isStopped, int job_id) {
    JobsList::removeFinishedJobs();
    int job_highest;
    if(job_id <=0) {
        job_highest = JobsList::getHighestJob();
        job_highest += 1;
        jobs_list.push_back(JobEntry(job_highest, pid, cmd,isStopped));
    }
    else{
        job_highest = job_id;
        jobs_list.insert(jobs_list.begin() + job_highest-1 ,JobEntry(job_highest, pid, cmd,isStopped));

    }

}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs):
        BuiltInCommand(cmd_line), jobs_list(jobs) {}


void JobsCommand::execute()
{
    /////note that job list need to be updated before calling this function
    this->jobs_list->removeFinishedJobs();
    this->jobs_list->printJobsList();
}


void JobsList::removeFinishedJobs() {
    int stat;
    vector<JobEntry>::iterator it= this->jobs_list.begin();
    while (it != this->jobs_list.end()) {
        int pid = it->j_process_id;
        if (waitpid(pid, &stat, WNOHANG) > 0 || kill(pid, 0) == -1) {
            delete it->j_command;
            it = this->jobs_list.erase(it);
        }
        if (it != this->jobs_list.end()) {
            it++;
        }
    }
//    if(jobs_list.size() == 0)
//    {
//        counter = 1;
//    }
}

void JobsList::printJobsList() {
    JobsList::removeFinishedJobs();
    int job_id =-1;
    string command ="";
    int p_id = -1;
    time_t time_passed = 0;
    for (unsigned int i = 0; i < this->jobs_list.size(); i++) {
        job_id = JobsList::jobs_list[i].j_id;
        command = JobsList::jobs_list[i].j_command->cmd_line;
        p_id= JobsList::jobs_list[i].j_process_id;
        time_passed = difftime(time(0), JobsList::jobs_list[i].time_job);
        bool isStopped = JobsList::jobs_list[i].is_stopped;
        if (isStopped)
        {
            cout << "[" << job_id << "] " << command << " : " << p_id << " " << time_passed <<" secs" << " (stopped)" << endl;
        }
        else {
            cout << "[" << job_id << "] " << command << " : " << p_id << " " << time_passed << " secs" << endl;
        }
    }
}
int JobsList::FindMaxStoppedJobIdInList(){
    vector<JobEntry>::iterator it= this->jobs_list.begin();
    int max_job_id = 0;
    while (it != this->jobs_list.end()) {
        if(it->is_stopped == true){
            max_job_id = it->j_id;
        }
        it++;
    }
    return max_job_id;
}

JobsList::JobEntry* JobsList::getJobById(int jobId)
{
    if (jobId < 0 || jobs_list.empty() || jobId > this->getHighestJob()) {
        return nullptr;
    }
    for(unsigned int i = 0; i < jobs_list.size(); i++)
    {
        if(jobs_list[i].j_id == jobId)
        {
            return &(jobs_list[i]);
        }
    }
    return nullptr;
}

JobsList::JobEntry* JobsList::getJobByPID(int j_process_id)
{
    if (j_process_id <= 0 || jobs_list.empty() ) { //TODO: this was also part of the if: || j_process_id > this->counter
        return nullptr;
    }
    for(unsigned int i = 0; i < jobs_list.size(); i++)
    {
        if(jobs_list[i].j_process_id == j_process_id)
        {
            return &(jobs_list[i]);
        }
    }
    return nullptr;
}



JobsList::JobEntry* JobsList::getLastJob() {
    int highest_job_id = getHighestJob();
    for (unsigned int i = 0; i < jobs_list.size(); i++) {
        if (jobs_list[i].j_id == highest_job_id) {
            return &jobs_list[i];

        }
    }
    return nullptr;
}

void JobsList::killAllJobs() {
    this->removeFinishedJobs();
    vector<JobEntry>::iterator it= this->jobs_list.begin();
    while (it != this->jobs_list.end()) {
        cout << it->j_process_id << ": " << it->j_command->cmd_line << endl;
        if (kill(it->j_process_id, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }
        else
        {
            delete it->j_command;
            it = jobs_list.erase(it);
        }
//        if (it != this->jobs_list.end()) {
//            it++;
//        }
    }
}


JobsList::JobEntry * JobsList::getLastStoppedJob(int *jobId) {
    int max_id_and_stopped = -1;
    for (unsigned int i = 0; i < jobs_list.size(); i++) {
        if ((jobs_list[i].j_id > max_id_and_stopped) && (jobs_list[i].is_stopped)) {
            max_id_and_stopped = jobs_list[i].j_id;
        }
    }
    if (max_id_and_stopped == -1) {
        return nullptr;
    } else {
        return getJobById(max_id_and_stopped);
    }
}


void JobsList::removeJobById(int jobId) {
    if (jobId <= 0 || jobs_list.empty() || jobId > this->getHighestJob())
        return;
    vector<JobEntry>::iterator it= this->jobs_list.begin();
    while (it != this->jobs_list.end()) {
        if (it->j_id == jobId) {
            delete it->j_command;
            it = jobs_list.erase(it);
            return;
        }
        if (it != jobs_list.end())
        {
            it++;
        }
    }
    return;
}


void JobsList::printJobsListKilled()
{
    auto it = this->jobs_list.begin();
    while (it!=this->jobs_list.end()) {
        cout << it->j_id << ": " << it->j_command->cmd_line << endl;
        it++;

    }
}


ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs):
        BuiltInCommand(cmd_line), jobs_list(jobs){}


void ForegroundCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry *selectedJob;
    if (this->args_size > 2 ) {
        //perror("smash error: fg: invalid arguments");
        cerr << "smash error: fg: invalid arguments" << endl;
        return;

    }

    if (this->args_size == 2) {

        if(this->arguments[1][0]!='-' &&!isDigitsOnly(this->arguments[1])){
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }

        selectedJob = this->jobs_list->getJobById(stoi(this->arguments[1]));
        if (selectedJob == nullptr) {
            string error = "smash error: fg: job-id ";
            error.append(this->arguments[1]);
            error.append(" does not exist");
            cerr << error<< endl;
            return;

        }
    }
    else { //no job_id arg
        selectedJob = this->jobs_list->getLastJob();
        if (selectedJob == nullptr)
        {
            cerr << "smash error: fg: jobs list is empty"<<endl;
            return;
        }
    }
    //selectedJob = this->jobs_list->getJobById(stoi(this->arguments[1]));
    cout << (selectedJob->j_command->cmd_line) << " : " << selectedJob->j_process_id << endl;

    if (selectedJob->is_stopped) {
        int res = kill(selectedJob->j_process_id, SIGCONT);
        if (res != 0) {
            perror("smash error: kill failed");
            exit(1);
        }
        selectedJob->is_stopped = false;
    }
    jobs_list->job_id_moves_from_bg_to_fg = selectedJob->j_id;
    int pid_selected_job = selectedJob->j_process_id;
    smash.current_fg_pid = pid_selected_job;
    int status;
    //int pid_to_run = selectedJob->j_process_id;
    if(waitpid(pid_selected_job , &status, WUNTRACED) == -1)
    {
        perror("smash error: waitpid failed");
        return;
    }

}




BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs)
        : BuiltInCommand(cmd_line), jobs_list(jobs) {}

void BackgroundCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry *selectedJob;
    if (this->args_size > 2) {
        cerr<<"smash error: bg: invalid arguments"<<endl;
        return;
    }
    if (this->args_size == 2) {
        if(this->arguments[1][0]!='-' &&!isDigitsOnly(this->arguments[1])){
            cerr << "smash error: bg: invalid arguments" << endl;
            return;
        }

        int jobId = atoi(this->arguments[1]);
        selectedJob = smash.jobs_list.getJobById(jobId);
        if (selectedJob == nullptr) {
            string error = "smash error: bg: job-id ";
            error.append(this->arguments[1]);
            error.append(" does not exist");
            cerr << error << endl;
            return;
        }
        if(selectedJob->is_stopped == false){
            cerr<< "smash error: bg: job-id "<< jobId <<" is already running in the background"<<endl;
            return;
        }
    }
    else if(this->args_size==1){
        int max_job_id = smash.jobs_list.FindMaxStoppedJobIdInList();
        if(max_job_id!=0){
            selectedJob = smash.jobs_list.getJobById(max_job_id);
        }
        else{
            cerr<<"smash error: bg: there is no stopped jobs to resume"<<endl;
            return;
        }
    }
    int res = kill(selectedJob->j_process_id, SIGCONT);
    if (res == -1) {
        cout << "smash doesnt kill!!!!!!" << endl;
        exit(1);
    }
    cout << selectedJob->j_command->cmd_line << " : " <<  selectedJob->j_process_id << endl;
    selectedJob->is_stopped = false;
    return;

}



bool isNumber(string str) {
    for (unsigned int i = 0; i < str.size(); i++) {
        if (!std::isdigit(str[i])) {
            return false;
        }
    }

    // the string represents a valid integer
    return true;
}


int isOctalDigit(string str) {
    for(char c : str) {
        if(!isdigit(c) || c < '0' || c > '7') {
            return false;
        }
    }
    return true;
}


//bool is_legit_signal_num(string word)
//{
//    int number = stoi(word);
//    if (number >= 0 && number <= 31)
//        return true;
//    return false;
//}
//
//bool is_legit_job_id(string word)
//{
//    int number = stoi(word);
//    if (number >= 0)
//        return true;
//    return false;
//}



KillCommand::KillCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}

void KillCommand::execute()
{
    if(args_size!=3){
        cerr<< "smash error: kill: invalid arguments" <<endl;
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    smash.jobs_list.removeFinishedJobs();
    string str_signal = string(this->arguments[1]);
    string signal_num = str_signal.substr(1,str_signal.length());//cut the "-" that comes before the number
    if((this->arguments[2][0] != '-'&&!isNumber(this->arguments[2])) ||
       this->arguments[1][0] != '-' || !isDigitsOnly(signal_num)){
        cerr<< "smash error: kill: invalid arguments" <<endl;
        return;
    }
    int job_id = stoi(this->arguments[2]);
    if(smash.jobs_list.getJobById(job_id)== nullptr){
        cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        return;
    }
    
    if(this->arguments[1][0]=='-'){
        int kill_num = stoi(string(this->arguments[1]).substr(1));
        if(kill_num<0 || kill_num>31){
            cerr<< "smash error: kill: invalid arguments" <<endl;
            return;
        }
    }

    int signal = stoi(signal_num);
    int job_pid = smash.jobs_list.getJobById(job_id)->j_process_id;

    if(kill(job_pid,signal)== -1)
    {
        perror("smash error : kill failed");
        return;
    }
    else{
        cout << "signal number " << signal << " was sent to pid " << job_pid << endl;
    }

    if(signal == SIGCONT)
    {
        smash.jobs_list.getJobById(job_id)->is_stopped = false;
        // return;
    }
    if(signal == SIGSTOP)
    {
        smash.jobs_list.getJobById(job_id)->is_stopped = true;
        //return;
    }
    if(signal == SIGKILL){
        smash.jobs_list.removeJobById(job_id);
    }
    if(signal == SIGTERM) {
        smash.jobs_list.removeJobById(job_id);
    }

}


QuitCommand::QuitCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}


void QuitCommand::execute()
{
    SmallShell& smash = SmallShell::getInstance();
    if (this->args_size > 1) {
        if (strcmp(arguments[1], "kill") == 0) {

            cout << "smash: sending SIGKILL signal to " << smash.jobs_list.getJobsListSize() << " jobs:" << endl;
            smash.jobs_list.killAllJobs();
        }
    }
    exit(0);
}

int  JobsList::getJobsListSize()
{
    SmallShell& smash = SmallShell::getInstance();
    smash.jobs_list.removeFinishedJobs();
    int counter =0;
    auto it = this->jobs_list.begin();
    while (it!=this->jobs_list.end()) {
        counter++;
        it++;
    }
    return counter;
}

bool IsComplexInExternal(string input)
{
    if ((input.find('*') != std::string::npos) || (input.find('?') != std::string::npos)) {
        return true;
    }
    return false;
}

void appendAmpersand(char* str) {
    strcat(str, "&");
}

void deleteLastChar(char* str) {
    if (std::strlen(str) > 0) {
        str[std::strlen(str) - 1] = '\0';
    }
}



ExternalCommand::ExternalCommand(const char* cmd_line): Command(cmd_line){}

void ExternalCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    int status;
    bool is_complicated = false;
    pid_t pid_wait;
    for (int i=0 ; i< args_size; i++)
    {
        if (IsComplexInExternal(arguments[i]))
        {
            is_complicated = true;
        }
    }
    bool is_background = _isBackgroundCommand(cmd_line);
    // now we know 4 states complicated and backgroud

    pid_t pid = fork(); // here we fork the current procces
    if (pid == -1)
    {
        perror("smash error: fork failed");
        return;
    }
    if (pid == 0) { // son
        if(setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            return;
        }

        if (is_complicated)
        {
            if (is_background)
            {
                _removeBackgroundSign(cmd_line);
                smash.current_fg_pid = -1; //TODO: do this to every background situation
            }

            char *bash_args[] = {
                    (char *) "/bin/bash",
                    (char *) "-c",
                    &cmd_line[0],
                    nullptr
            };
            if(execvp(bash_args[0], bash_args) == -1)
            {
                perror("smash error: execvp failed");
                return;
            }
        }
        else
        {
            if (is_background)
            {
                _removeBackgroundSign(cmd_line);
                smash.current_fg_pid = -1; //TODO: do this to every background situation
            }
//            else{
//                smash.current_fg_pid = getpid(); //TODO:make sure its the correct way
//            }
            char** cmd_args = new char *[MAX_ARGS_IN_CMD];
            _parseCommandLine(this->cmd_line,cmd_args);
            if(execvp(cmd_args[0], cmd_args) ==-1)
            {
                perror("smash error: execvp failed");
                return;
            }
        }
    }
    else // father
    {
        if(is_background) {
            //appendAmpersand(this->cmd_line);
            //deleteLastChar(this->cmd_line);
            //  cout << this->cmd_line << endl;
            smash.jobs_list.addJob(this, pid, false);
        }
        else{
            smash.current_fg_pid = pid; //TODO:make sure its the correct way
            pid_wait = waitpid(pid,&status, WUNTRACED);
            //smash.current_fg_pid = -1; //TODO:check this is the right place - for fg
            if(pid_wait == -1)
            {
                perror("smash error: waitpid failed");
                return;
            }
        }
    }

}


#define RD 0
#define WT 1
#define STDERR 2
PipeCommand::PipeCommand(const char *cmd_line): Command(cmd_line) {}

void PipeCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    string second_command = "";
    string pipe_symbol = "";
    int pipe_fd[2];
    string first_command = string(cmd_line).substr(0, string(cmd_line).find("|"));
    if (string(cmd_line).find("|&") != std::string::npos) {
        pipe_symbol = "|&";
        second_command = string(cmd_line).substr(string(cmd_line).find("|&") + 2);
    } else {
        pipe_symbol = "|";
        second_command = string(cmd_line).substr(string(cmd_line).find("|") + 1);
    }

    int pipeChannel;
    if (pipe_symbol == "|") {
        pipeChannel = 1;
    } else //symbol is |&
    {
        pipeChannel = 2;
    }

    if(pipe(pipe_fd) != 0 ) {
        perror("smash error: pipe failed");
        return;
    }

    int pid = fork();
    if (pid == 0) {
        setpgrp();

        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], pipeChannel) == -1) {
            perror("smash error: dup failed");
            exit(0);
        }

        Command *cmd1 = smash.CreateCommand(first_command.c_str());
        if (cmd1 == nullptr) {
            delete cmd1;
        }
        if (_isBackgroundCommand(cmd_line))
        {
            smash.current_fg_pid = -1;
        }
        cmd1->execute();

        if (close(pipe_fd[1]) == -1) {
            perror("smash error: close failed");
        }

        exit(0);
    }
    else {
        close(pipe_fd[1]);
        int old_out = dup(0);
        if (old_out == -1) {
            perror("smash error: dup failed");
            return;
        }
        if (dup2(pipe_fd[0], 0) == -1) {
            perror("smash error: dup failed");
            return;
        }
        Command *cmd = smash.CreateCommand(second_command.c_str());
        if (cmd == nullptr) {
            delete cmd;
        }
        if (_isBackgroundCommand(cmd_line))
        {
            smash.current_fg_pid = -1;
        }

        cmd->execute();
        if (close(pipe_fd[0]) == -1) {
            perror("smash error: close failed");
        }
        if (dup2(old_out, 0) == -1) {
            perror("smash error: dup failed");
            return;
        }
        if(close(old_out) == -1){
            perror("smash error: close failed");
        }

        int status;
        if(waitpid(pid,&status, WUNTRACED) == -1)
        {
            perror("smash error: waitpid failed");
            return;
        }
    }
}



GetFileTypeCommand::GetFileTypeCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
//TODO:check if this command should inherit from BuiltIn

void GetFileTypeCommand::execute() {
    char* path = this->arguments[1];
    if((this->args_size>2 && string(this->arguments[2]).find('&')== string::npos)|| this->args_size <2){
        cerr<<"smash error: getfiletype: invalid arguments"<<endl;
        return;
    }
    else{
        struct stat file_info;
        if(lstat(this->arguments[1],&file_info)==-1){
            perror("smash error: lstat failed");
            return;
        }
        else{
            string type;
            mode_t mode = file_info.st_mode;

            if(S_ISREG(mode)) {
                type = "regular file";
            }
            else if(S_ISDIR(mode)) {
                type = "directory";
            }
            else if(S_ISLNK(mode)) {
                type = "symbolic link";
            }
            else if(S_ISFIFO(mode)) {
                type = "FIFO";
            }
            else if(S_ISSOCK(mode)) {
                type = "socket";
            }
            else if(S_ISCHR(mode)) {
                type = "character device";
            }
            else if(S_ISBLK(mode)) {
                type = "block device";
            }
            else{
                type = "Unknown";
            }
            cout<<path<<"'s type is "<<'"'<<type<<'"'<<" and takes up "<<file_info.st_size<<" Byte"<<endl;
        }
    }
}

SetcoreCommand::SetcoreCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}

void SetcoreCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    if(this->args_size!=3 ||  !isNumber(this->arguments[1]) || !isNumber(this->arguments[2])){
        cerr<<"smash error: setcore: invalid arguments"<<endl;
        return;
    }
    int curr_num_of_cores = get_nprocs();
    if(stoi(this->arguments[2]) > curr_num_of_cores || strlen(this->arguments[2]) > 1 ){
        cerr<<"smash error: setcore: invalid core number"<<endl;
        return;
    }
    JobsList::JobEntry* curr_job = smash.jobs_list.getJobById(stoi(this->arguments[1]));
    if(curr_job == nullptr){
        cerr<<"smash error: setcore: job-id "<<this->arguments[1]<<" does not exist"<<endl;
        return;
    }

    int pid = curr_job->j_process_id;
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(stoi(this->arguments[2]), &cpu_set); //TODO:check this is the correct way to do this
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) == -1) {
        perror("smash error: sched_setaffinity failed");
        return;
    }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line){}

void RedirectionCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    bool is_append = false;
    if(string(cmd_line).find(">>") != std::string::npos){
        is_append = true;
    }

    //split the command itself and the file name we want to insert the data to
    string cmd = (string(cmd_line).substr(0,string(cmd_line).find(">")));
    string file_name = _trim(string(cmd_line).substr(string(cmd_line).find_last_of(">") + 1));

    int output_file_new, original_stdout;

    if (is_append) {
        output_file_new = open(file_name.c_str() , O_CREAT | O_WRONLY | O_APPEND , 0666);
        //TODO:check that the 0666 is working in this part
    }
    else {
        output_file_new = open(file_name.c_str() , O_CREAT | O_WRONLY | O_TRUNC, 0666);
    }

    if(output_file_new == -1){
        perror("smash error: open failed");
        return;
    }

    original_stdout = dup(1);
    if(original_stdout == -1) {
        perror("smash error: dup failed");
        return;
    }

    int res = dup2(output_file_new,1);
    if(res == -1){
        perror("smash error: dup failed");
        return;
    }

    res = close(output_file_new);
    if(res == -1){
        perror("smash error: close failed");
        return;
    }

    //return everything to the way it was:
    smash.executeCommand(cmd.c_str());
    res = dup2(original_stdout,1);
    if(res == -1){
        perror("smash error: dup failed");
        return;
    }

    res = close(original_stdout);
    if(res == -1){
        perror("smash error: close failed");
        return;
    }
}

ChmodCommand::ChmodCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void ChmodCommand::execute() {
    if(this->args_size != 3 || !isOctalDigit(this->arguments[1])){
        cerr<<"smash error: chmod: invalid arguments"<<endl;
        return;
    }

    long int mode = strtol(this->arguments[1],NULL, 8);
    string path = this->arguments[2];

    if(chmod(path.c_str(),mode) == -1){
        perror("smash error: chmod failed");
        return;
    }
}


//void SmallShell::handle_alarm() {}

