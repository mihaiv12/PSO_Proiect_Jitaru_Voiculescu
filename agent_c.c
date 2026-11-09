#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <dirent.h>
    #include <pwd.h>
    #include <sys/utsname.h>
#else
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #if defined(_MSC_VER)
        typedef intptr_t ssize_t;
    #endif
#endif

// ==========================================
// 1. MODELE DE DATE (Model)
// ==========================================

struct SystemInfo {
    char hostname[64];
    char os_name[64];
    char user[64];
    double cpu_percent;
    long ram_used_mb;
    long ram_total_mb;
    long uptime_seconds;
};

struct ProcessInfo {
    int pid;
    char name[64];
    char state;
    char user[64];
    unsigned long utime;
    unsigned long stime;
    unsigned long long ctime;
};

static volatile int g_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    printf("\n[AGENT] Semnal de oprire primit (Ctrl+C)...\n");
    g_running = 0;
}

// ==========================================
// 2. COLECTARE DATE DIN /proc (Collectors)
// ==========================================

void get_cpu_times(unsigned long long* idle, unsigned long long* total) {
    *idle = 0;
    *total = 0;
#ifndef _WIN32
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return;

    char cpu[16];
    unsigned long long user, nice, system, idle_t, iowait, irq, softirq, steal;
    if (fscanf(f, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
               cpu, &user, &nice, &system, &idle_t, &iowait, &irq, &softirq, &steal) == 9) {
        *idle = idle_t + iowait;
        *total = user + nice + system + idle_t + iowait + irq + softirq + steal;
    }
    fclose(f);
#endif
}

double get_cpu_percent(void) {
#ifndef _WIN32
    unsigned long long idle1, total1, idle2, total2;
    get_cpu_times(&idle1, &total1);
    usleep(100000); // pauza de 100 ms
    get_cpu_times(&idle2, &total2);

    unsigned long long diff_total = total2 - total1;
    unsigned long long diff_idle = idle2 - idle1;
    if (diff_total == 0) return 0.0;
    return (1.0 - (double)diff_idle / (double)diff_total) * 100.0;
#else
    return 5.0;
#endif
}

void get_memory_info(long* total_mb, long* used_mb) {
    *total_mb = 0;
    *used_mb = 0;
#ifndef _WIN32
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return;

    char line[128];
    long mem_total = 0, mem_avail = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%ld", &mem_total);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%ld", &mem_avail);
        }
    }
    fclose(f);

    *total_mb = mem_total / 1024;
    *used_mb = (mem_total - mem_avail) / 1024;
#else
    *total_mb = 8192;
    *used_mb = 4096;
#endif
}

long get_system_uptime(void) {
#ifndef _WIN32
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up = 0;
    if (fscanf(f, "%lf", &up) == 1) {
        fclose(f);
        return (long)up;
    }
    fclose(f);
#endif
    return 0;
}

void collect_system_info(struct SystemInfo* info) {
    memset(info, 0, sizeof(*info));

#ifndef _WIN32
    if (gethostname(info->hostname, sizeof(info->hostname)) != 0) {
        strncpy(info->hostname, "linux-host", sizeof(info->hostname) - 1);
    }

    struct utsname un;
    if (uname(&un) == 0) {
        snprintf(info->os_name, sizeof(info->os_name), "%s %s", un.sysname, un.release);
    } else {
        strncpy(info->os_name, "Linux", sizeof(info->os_name) - 1);
    }

    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        strncpy(info->user, pw->pw_name, sizeof(info->user) - 1);
    } else {
        strncpy(info->user, "unknown", sizeof(info->user) - 1);
    }
#else
    strncpy(info->hostname, "localhost", sizeof(info->hostname) - 1);
    strncpy(info->os_name, "Windows", sizeof(info->os_name) - 1);
    strncpy(info->user, "user", sizeof(info->user) - 1);
#endif

    info->cpu_percent = get_cpu_percent();
    get_memory_info(&info->ram_total_mb, &info->ram_used_mb);
    info->uptime_seconds = get_system_uptime();
}

// Citeste proprietarul unui PID din /proc/[pid]/status
void get_pid_username(int pid, char* out_user, size_t max_len) {
    strncpy(out_user, "unknown", max_len - 1);
#ifndef _WIN32
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            int ruid = -1;
            if (sscanf(line + 4, "%d", &ruid) == 1) {
                struct passwd* pw = getpwuid(ruid);
                if (pw && pw->pw_name) {
                    strncpy(out_user, pw->pw_name, max_len - 1);
                } else {
                    snprintf(out_user, max_len, "%d", ruid);
                }
                break;
            }
        }
    }
    fclose(f);
#else
    (void)pid;
#endif
}

// Citeste datele unui proces din /proc/[pid]/stat
int read_process_stat(int pid, struct ProcessInfo* proc) {
    memset(proc, 0, sizeof(*proc));
    proc->pid = pid;

#ifndef _WIN32
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    // Numele procesului este delimitat de paranteze: (nume)
    char* open_p = strchr(line, '(');
    char* close_p = strrchr(line, ')');
    if (!open_p || !close_p || close_p <= open_p) return 0;

    size_t name_len = close_p - (open_p + 1);
    if (name_len >= sizeof(proc->name)) name_len = sizeof(proc->name) - 1;
    strncpy(proc->name, open_p + 1, name_len);
    proc->name[name_len] = '\0';

    // Extragem starea, utime, stime si starttime (ctime) dupa paranteza
    char state = '?';
    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned long flags, minflt, cminflt, majflt, cmajflt;
    unsigned long utime = 0, stime = 0;
    long cutime, cstime, priority, nice, num_threads, itrealvalue;
    unsigned long long starttime = 0;

    if (sscanf(close_p + 2, "%c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu",
               &state, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,
               &minflt, &cminflt, &majflt, &cmajflt,
               &utime, &stime, &cutime, &cstime,
               &priority, &nice, &num_threads, &itrealvalue, &starttime) >= 1) {
        proc->state = state;
        proc->utime = utime;
        proc->stime = stime;
        proc->ctime = starttime;
    }

    get_pid_username(pid, proc->user, sizeof(proc->user));
    return 1;
#else
    strncpy(proc->name, "init", sizeof(proc->name) - 1);
    proc->state = 'S';
    strncpy(proc->user, "root", sizeof(proc->user) - 1);
    return 1;
#endif
}

// ==========================================
// 3. CAPTURA DE ECRAN (Screenshot)
// ==========================================

unsigned char* generate_placeholder_bmp(int* out_size) {
    int width = 320;
    int height = 240;
    int row_stride = ((width * 3 + 3) / 4) * 4;
    int image_size = row_stride * height;
    int file_size = 54 + image_size;

    unsigned char* bmp = (unsigned char*)calloc(1, file_size);
    if (!bmp) return NULL;

    // Header BMP (14 octeti)
    bmp[0] = 'B';
    bmp[1] = 'M';
    memcpy(&bmp[2], &file_size, 4);
    int offset = 54;
    memcpy(&bmp[10], &offset, 4);

    // DIB Header (40 octeti)
    int dib_size = 40;
    memcpy(&bmp[14], &dib_size, 4);
    memcpy(&bmp[18], &width, 4);
    memcpy(&bmp[22], &height, 4);
    unsigned short planes = 1;
    memcpy(&bmp[26], &planes, 2);
    unsigned short bpp = 24;
    memcpy(&bmp[28], &bpp, 2);
    memcpy(&bmp[34], &image_size, 4);

    // Desenam un gradient albastru simplu
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = 54 + y * row_stride + x * 3;
            bmp[idx] = (unsigned char)(x % 255);     // Blue
            bmp[idx + 1] = (unsigned char)(y % 255); // Green
            bmp[idx + 2] = 40;                       // Red
        }
    }

    *out_size = file_size;
    return bmp;
}

// ==========================================
// 4. RETEA SI PROTOCOL SIMPLU (Network)
// ==========================================

int connect_to_server(const char* ip, int port) {
#ifdef _WIN32
    static int wsa_init = 0;
    if (!wsa_init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        wsa_init = 1;
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
#ifndef _WIN32
        close(sock);
#else
        closesocket(sock);
#endif
        return -1;
    }

    return sock;
}

void close_socket(int sock) {
    if (sock >= 0) {
#ifndef _WIN32
        close(sock);
#else
        closesocket(sock);
#endif
    }
}

int send_text(int sock, const char* text) {
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s\n", text);
    ssize_t sent = send(sock, buf, (int)strlen(buf), 0);
    return (sent > 0);
}

int send_binary(int sock, const unsigned char* data, int size) {
    uint32_t net_len = htonl((uint32_t)size);
    if (send(sock, (const char*)&net_len, 4, 0) <= 0) return 0;

    int total = 0;
    while (total < size) {
        ssize_t n = send(sock, (const char*)data + total, size - total, 0);
        if (n <= 0) return 0;
        total += n;
    }
    return 1;
}

// ==========================================
// 5. LOGICA PRINCIPALA A AGENTULUI (Main loop)
// ==========================================

void send_system_report(int sock, const char* agent_id) {
    struct SystemInfo sys;
    collect_system_info(&sys);

    printf("--- [Telemetrie] Host: %s | User: %s | CPU: %.1f%% | RAM: %ld/%ld MB ---\n",
           sys.hostname, sys.user, sys.cpu_percent, sys.ram_used_mb, sys.ram_total_mb);

    char msg[512];
    snprintf(msg, sizeof(msg), "INFO|%s|%s|%s|%s|%.2f|%ld|%ld|%ld",
             agent_id, sys.hostname, sys.os_name, sys.user,
             sys.cpu_percent, sys.ram_used_mb, sys.ram_total_mb, sys.uptime_seconds);

    send_text(sock, msg);
}

void send_process_report(int sock, const char* agent_id) {
    char packet[4096];
    int count = 0;
    snprintf(packet, sizeof(packet), "PROCS|%s|", agent_id);

#ifndef _WIN32
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL && count < 30) {
            if (isdigit(entry->d_name[0])) {
                int pid = atoi(entry->d_name);
                struct ProcessInfo proc;
                if (read_process_stat(pid, &proc)) {
                    char entry_str[128];
                    snprintf(entry_str, sizeof(entry_str), "%d:%s:%c:%s:%lu:%lu:%llu;",
                             proc.pid, proc.name, proc.state, proc.user,
                             proc.utime, proc.stime, proc.ctime);

                    if (strlen(packet) + strlen(entry_str) < sizeof(packet) - 1) {
                        strcat(packet, entry_str);
                        count++;
                    }
                }
            }
        }
        closedir(dir);
    }
#else
    strcat(packet, "1:init:S:root:100:50:1;");
    count = 1;
#endif

    printf("  [Procese] Trimis informatii despre %d procese active\n", count);
    send_text(sock, packet);
}

void send_screen_report(int sock, const char* agent_id) {
    int bmp_size = 0;
    unsigned char* bmp_data = generate_placeholder_bmp(&bmp_size);
    if (!bmp_data) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "SCREEN|%s", agent_id);
    if (send_text(sock, msg)) {
        if (send_binary(sock, bmp_data, bmp_size)) {
            printf("  [Captura Ecran] Cadru transmis cu succes (%d octeti)\n", bmp_size);
        }
    }
    free(bmp_data);
}

// ==========================================
// 6. ENTRY POINT
// ==========================================

int main(int argc, char* argv[]) {
    const char* agent_id = "agent_c_01";
    const char* server_ip = "127.0.0.1";
    int server_port = 9000;
    int interval_sec = 5;

    // Sintaxa linie de comanda: ./agent_c [agent_id] [ip] [port] [interval]
    if (argc > 1) agent_id = argv[1];
    if (argc > 2) server_ip = argv[2];
    if (argc > 3) server_port = atoi(argv[3]);
    if (argc > 4) interval_sec = atoi(argv[4]);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    printf("========================================\n");
    printf("  Agent Minimal Linux (Scris in C)\n");
    printf("  ID Agent : %s\n", agent_id);
    printf("  Server   : %s:%d\n", server_ip, server_port);
    printf("  Interval : %d secunde\n", interval_sec);
    printf("========================================\n\n");

    int sock = -1;
    int cycle = 0;

    // Bucla principala de functionare si reconectare automata
    while (g_running) {
        // 1. Daca nu este conectat, incercam reconectarea la server
        if (sock < 0) {
            printf("[CONECTARE] Incercare de conectare la %s:%d...\n", server_ip, server_port);
            sock = connect_to_server(server_ip, server_port);
            if (sock < 0) {
                printf("[CONECTARE] Serverul nu raspunde. Reincercare in 3 secunde...\n");
#ifndef _WIN32
                sleep(3);
#else
                Sleep(3000);
#endif
                continue;
            }

            printf("[CONECTARE] Conectat la server cu succes!\n");
            // Trimitem mesajul de inregistrare
            char reg_msg[128];
            snprintf(reg_msg, sizeof(reg_msg), "REGISTER|%s", agent_id);
            if (!send_text(sock, reg_msg)) {
                close_socket(sock);
                sock = -1;
                continue;
            }
        }

        // 2. Trimitem raportul de telemetrie si procese
        send_system_report(sock, agent_id);
        send_process_report(sock, agent_id);

        // 3. Trimitem o captura de ecran la fiecare 3 cicluri
        cycle++;
        if (cycle % 3 == 0) {
            send_screen_report(sock, agent_id);
        }

        // 4. Pauza pana la urmatorul ciclu
#ifndef _WIN32
        sleep(interval_sec);
#else
        Sleep(interval_sec * 1000);
#endif
    }

    close_socket(sock);
    printf("[AGENT] Terminat curat.\n");
    return 0;
}
