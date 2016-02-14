#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <errno.h>
#include "configini.h"

#define CONFIG_FILE "settings.cfg"

#define MSI_SERVICE_NAME "MSISuperIO_CC"
#define MSI_WM_SET_FAN_TEMP_CONTROL 0xB3
#define MSI_CONFIG_FILE "C:\\MSI\\Command Center\\MSISuperIO.cfg"

#define MAX(a,b) (a>b?a:b)

struct Settings {
    unsigned int timeToWaitForServiceToStart;
    unsigned int timeBetweenFanCommands;
};

void printLastError();

WINBOOL openConfigFile(Config** cfg) {
    ConfigRet ret = ConfigReadFile(MSI_CONFIG_FILE, cfg);
    if (ret != CONFIG_OK) {
        if (ret == CONFIG_ERR_INVALID_PARAM) {
            printf("Invalid param");
        } else if (ret == CONFIG_ERR_FILE) {
            printf("Unable to open cfg file!");
        } else {
            printf("Unable to read config file! >%d/%x<", ret, ret);
            printLastError();
        }
        return FALSE;
    } else {
        printf("Config read!\n");
        return TRUE;
    }
}

void showHelp() {
    printf("This program sets the fan speed of MSI boards.\n");
    printf("The argument must have the following layout where the <> are variables:\n");
    printf("<FAN>;<S1>;<S2>;<S3>;<S4>;<T1>;<T2>;<T3>;<T4>;77\n");
    printf("<FAN> is the fan id. 1 and 2 are most likely the cpu fans do dont touch them!\n");
    printf("In most cases 8 and 9 and 3 are case fans which are the fans for me on H97.\n");
    printf("I have not really a clue how you could figure the ids out.\n");
    printf("I also not sure if you can damage something when entering a wrong one.\n");
    printf("<Sx> is the desired speed of the fan and <Tx> is the temperature.\n");
    printf("The 'x' is the number of the 'Ball' you can move in the Command Center, numbered from the left to the right.\n");
    printf("However, the <Sx> and <Tx> values need to be entered as HEX (uppercase) and not as decimal numbers.\n");
    printf("As an example, this would set my one of my front fans to an acceptable speed:\n");
    printf("8;1E;23;2D;3C;2D;37;41;50;77;\n");
    printf("[30/45],[35,55],[45,65],[55,80] where [Speed/Temp]\n");
    printf("The last 77 might be the temp where the 100 speed triggers but this is only speculation.\n");
    printf("And yes, this bypasses the GUI which might show nothing or completely off values.\n");
    printf("\nBY USING THIS PROGAM YOU ACKNOWLEDGE THAT YOU ARE USING IT AT YOUR OWN RISK AND I AM NOT RESPONSIBLE FOR ANY DAMAGES IN ANY WAY!");
    printf("This program proudly uses libconfigini (https://github.com/taneryilmaz/libconfigini/)");
}

void readSettings(struct Settings* settings) {
    Config *cfg = NULL;
    ConfigRet ret = ConfigReadFile(CONFIG_FILE, &cfg);

    if (ret == CONFIG_OK) {
        ConfigReadInt(cfg, "sleep", "timeToWaitForServiceToStart", &settings->timeToWaitForServiceToStart, 2000);
        ConfigReadInt(cfg, "sleep", "timeBetweenFanCommands", &settings->timeBetweenFanCommands, 1000);
        ConfigFree(cfg);
    } else {
        settings->timeToWaitForServiceToStart = 2000;
        settings->timeBetweenFanCommands = 1000;
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        showHelp();
        return 0;
    }
    Config *cfg = NULL;
    int error = 0;

    struct Settings settings;
    readSettings(&settings);

    //Fix for fopen not able to write hidden file
    SetFileAttributesA(MSI_CONFIG_FILE, FILE_ATTRIBUTE_NORMAL);

    openConfigFile(&cfg);

    WINBOOL letServiceRunning = FALSE;

    SC_HANDLE serviceControl = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (serviceControl == NULL) {
        printf("OpenSCManagerA: ");
        printLastError();
        return 1;
    }

    //Try to start the server
    SC_HANDLE service = OpenServiceA(serviceControl, MSI_SERVICE_NAME, SERVICE_START | SERVICE_QUERY_STATUS);
    if (service != NULL) {
        if (StartServiceA(service, 0, NULL) != 0) {
            SERVICE_STATUS sStatus;
            do {
                if (QueryServiceStatus(service, &sStatus) != 0) {
                    Sleep(MAX(sStatus.dwWaitHint, settings.timeToWaitForServiceToStart));
                } else {
                    printf("Unable to read service status: ");
                    printLastError();
                    break;
                }
            } while (sStatus.dwCurrentState == SERVICE_START_PENDING
                    || sStatus.dwCurrentState == SERVICE_CONTINUE_PENDING);
            if (sStatus.dwCurrentState == SERVICE_RUNNING) {
                printf("Started service\n");
            } else {
                printf("Unable to start service! State is %d.\n", sStatus.dwCurrentState);
            }
        } else {
            if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
                letServiceRunning = TRUE;
                printf("Service is already running, not touching state.\n");
            } else {
                printf("Could not start service (skipping): ");
                printLastError();
            }
        }
        CloseServiceHandle(service);
    } else {
        printf("Could not open service for starting (skipping): ");
        printLastError();
    }

    //Open the service for communication
    service = OpenServiceA(serviceControl, MSI_SERVICE_NAME, SERVICE_USER_DEFINED_CONTROL);
    if (service == NULL) {
        printf("Open service for control: ");
        printLastError();
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        ConfigAddString(cfg, "hw_setFanTempControl", "FanControl", argv[i]);

        ConfigRet ret = ConfigPrintToFile(cfg, MSI_CONFIG_FILE);
        if (ret != CONFIG_OK) {
            printf("Unable to write to file %s! >%d/%x<\n", MSI_CONFIG_FILE, ret, ret);
            printf("Errno is %d\n", errno);
            error = 1;
            break;
        }

        //Sending the command
        SERVICE_STATUS ssStatus;
        if (ControlService(service, MSI_WM_SET_FAN_TEMP_CONTROL, &ssStatus)) {
            printf("COMMAND SUCCESSFULLY SENT!\n");
            printf("Status: %d\n", ssStatus.dwCurrentState);
            //Give the service some time to act. Probably not needed
            //but better safe than sorry
            Sleep(settings.timeBetweenFanCommands);
        } else {
            printf("COMMAND NOT SENT!\n");
            printLastError();
            error = 2;
            break;
        }
    }
    ConfigFree(cfg);

    CloseServiceHandle(service);

    //Stop the service if it was not running before
    SERVICE_STATUS ssStatus;
    if (!letServiceRunning) {
        service = OpenServiceA(serviceControl, MSI_SERVICE_NAME, SERVICE_STOP);
        ControlService(service, SERVICE_CONTROL_STOP, &ssStatus);
        CloseServiceHandle(service);
        printf("Stopped service");
    }

    CloseServiceHandle(serviceControl);
    return (EXIT_SUCCESS);
}

void printLastError() {
    char* s = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR) & s, 0, NULL);
    printf("Error %d: %s\n", GetLastError(), s);
    LocalFree(s);
}