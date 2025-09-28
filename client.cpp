/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Dylan Myers
	UIN: 633007601
	Date: 9/28/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>

using namespace std;

int main (int argc, char *argv[]) {
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	bool c_flag = false;

	string filename = "";
    while ((opt = getopt(argc, argv, "p:t:e:f:c")) != -1) {
        switch (opt) {
        case 'p':
            p = atoi(optarg);
            break;
        case 't':
            t = atof(optarg);
            break;
        case 'e':
            e = atoi(optarg);
            break;
        case 'f':
            filename = optarg;
            break;
        case 'c':
            c_flag = true;
            break;
        }
    }

    pid_t server_pid = fork();
    if (server_pid == 0) {
        execl("./server", "server", (char*)NULL);
        perror("execl failed");
        exit(1);
    } else if (server_pid < 0) {
        perror("fork failed");
        exit(1);
    }

    sleep(1);

    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel* data_chan = nullptr;

    if (c_flag) {
        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        chan.cwrite(&m, sizeof(MESSAGE_TYPE));
        char new_chan[30];
        chan.cread(new_chan, sizeof(new_chan));
        data_chan = new FIFORequestChannel(new_chan, FIFORequestChannel::CLIENT_SIDE);
    }

    FIFORequestChannel* working_chan = data_chan ? data_chan : &chan;

    if (!filename.empty()) {
        auto start_time = chrono::high_resolution_clock::now();

        filemsg size_request(0, 0);
        int request_size = sizeof(filemsg) + filename.size() + 1;
        char* request_buffer = new char[request_size];
        memcpy(request_buffer, &size_request, sizeof(filemsg));
        strcpy(request_buffer + sizeof(filemsg), filename.c_str());

        working_chan->cwrite(request_buffer, request_size);
        __int64_t file_size;
        working_chan->cread(&file_size, sizeof(__int64_t));
        delete[] request_buffer;

        mkdir("received", 0777);
        string output_path = "received/" + filename;
        ofstream outfile(output_path, ios::binary);

        __int64_t bytes_received = 0;
        int chunk_size = MAX_MESSAGE - sizeof(filemsg) - filename.size() - 1;
        char* data_buffer = new char[MAX_MESSAGE];

        while (bytes_received < file_size) {
            int bytes_to_request = min((__int64_t)chunk_size, file_size - bytes_received);

            filemsg chunk_request(bytes_received, bytes_to_request);
            request_size = sizeof(filemsg) + filename.size() + 1;
            char* chunk_request_buffer = new char[request_size];
            memcpy(chunk_request_buffer, &chunk_request, sizeof(filemsg));
            strcpy(chunk_request_buffer + sizeof(filemsg), filename.c_str());

            working_chan->cwrite(chunk_request_buffer, request_size);
            int bytes_read = working_chan->cread(data_buffer, bytes_to_request);

            outfile.write(data_buffer, bytes_read);
            bytes_received += bytes_read;

            delete[] chunk_request_buffer;
        }

        outfile.close();
        delete[] data_buffer;

        auto end_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);

        cout << "File transfer completed in " << duration.count() << " ms\n";
        cout << "File saved as: " << output_path << "\n";
    }
    else if (filename.empty() && t == 0.0 && e == 1) {
        ofstream outfile("x1.csv");
        outfile << "Time,ECG1,ECG2\n";

        for (int i = 0; i < 1000; i++) {
            double time = i * 0.004;

            datamsg req1(p, time, 1);
            working_chan->cwrite(&req1, sizeof(datamsg));
            double ecg1;
            working_chan->cread(&ecg1, sizeof(double));

            datamsg req2(p, time, 2);
            working_chan->cwrite(&req2, sizeof(datamsg));
            double ecg2;
            working_chan->cread(&ecg2, sizeof(double));

            outfile << time << ',' << ecg1 << ',' << ecg2 << "\n";
        }

        outfile.close();
        cout << "Generated x1.csv\n";
    }
    else if (t > 0.0) {
        datamsg req(p, t, e);
        working_chan->cwrite(&req, sizeof(datamsg));
        double ret;
        working_chan->cread(&ret, sizeof(double));

        cout << "Person " << p << "'s ecg value " << e << " at time " << t << " is " << ret << "\n";
    }

    if (data_chan) {
        MESSAGE_TYPE quit_msg = QUIT_MSG;
        data_chan->cwrite(&quit_msg, sizeof(MESSAGE_TYPE));
        delete data_chan;
    }

    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));

    int status;
    waitpid(server_pid, &status, 0);
    cout << "Server terminated\n";

    return 0;
}