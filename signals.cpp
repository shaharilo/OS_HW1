#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    ////Ctrl+Z causes the shell to send SIGTSTP to the process running in the foreground
    SmallShell& smash = SmallShell::getInstance();
    cout << "smash: got ctrl-Z" << endl;
    if (smash.current_fg_pid < 0 )
    {
        return;
    }
    else {
        JobsList::JobEntry *selectedJob = smash.jobs_list.getJobByPID(smash.current_fg_pid);
        ExternalCommand* command;
        if(selectedJob== nullptr){
            //meaning it never was on the job list
            command = new ExternalCommand(smash.cmd_running.c_str());
        }
        else{
            string cmd_to_stop = selectedJob->j_command->cmd_line;
            selectedJob->is_stopped = true;
            command = new ExternalCommand(cmd_to_stop.c_str());
            //smash.jobs_list.removeJobById(selectedJob->j_id);

        }
        cout << "smash: process " << smash.current_fg_pid << " was stopped" << endl;
        //string cmd_to_stop = smash.cmd_running;
        kill(smash.current_fg_pid, SIGSTOP);
        smash.jobs_list.addJob(command , smash.current_fg_pid, true,smash.jobs_list.job_id_moves_from_bg_to_fg);
        smash.jobs_list.job_id_moves_from_bg_to_fg = -1;
        smash.current_fg_pid = -1;
    }
}

void ctrlCHandler(int sig_num) {
    //// Ctrl+C causes the shell to send SIGINT to the process running in the foreground
    SmallShell& smash = SmallShell::getInstance();
    cout << "smash: got ctrl-C" << endl;
    if(smash.current_fg_pid >= 0) {
        //smash.jobs_list.getJobByPID(smash.current_fg_pid)->is_stopped = true;
        //// maybe signal here is not right
        kill(smash.current_fg_pid, SIGKILL);
        cout << "smash: process " << smash.current_fg_pid << " was killed" << endl;
        smash.current_fg_pid = -1;
    }
    else{
        return;
    }
}

void alarmHandler(int sig_num) {
    // TODO: Add your implementation
    SmallShell& smash = SmallShell::getInstance();
    cout << "smash: got an alarm" << endl;
//    smash.
//    ((SmallShell*)running_smash)->handle_alarm();


}

