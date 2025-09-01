#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <zip.h>

#define PASSWORD_LENGTH 6

typedef struct {
    const char* zip_path;
    int start;
    int end;
    int thread_id;
} ThreadData;

volatile int found = 0;
char found_password[PASSWORD_LENGTH + 1];
pthread_mutex_t lock;

int try_password(zip_t* za, const char* password) {
    if (zip_set_default_password(za, password) < 0) {
        return 0;
    }
    zip_int64_t num_entries = zip_get_num_entries(za, 0);
    int success = 0;
    for (zip_uint64_t i = 0; i < num_entries; i++) {
        struct zip_stat st;
        if (zip_stat_index(za, i, 0, &st) != 0 || st.size == 0) continue;
        zip_file_t* zf = zip_fopen_index(za, i, 0);
        if (!zf) continue;
        char buffer[16 * 1024];
        zip_int64_t read_bytes = zip_fread(zf, buffer, sizeof(buffer));
        if (read_bytes > 0) {
            // 前16KB读取成功，再尝试完整读取确认
            zip_int64_t total_read = read_bytes;
            char* full_buffer = malloc(st.size);
            if (!full_buffer) {
                zip_fclose(zf);
                continue;
            }
            memcpy(full_buffer, buffer, read_bytes);
            while (total_read < st.size) {
                zip_int64_t chunk = zip_fread(zf, full_buffer + total_read, st.size - total_read);
                if (chunk <= 0) {
                    success = 0;
                    break;
                }
                total_read += chunk;
            }
            free(full_buffer);
            if (total_read == st.size) {
                success = 1;
                zip_fclose(zf);
                break;
            }
        }
        zip_fclose(zf);
    }

    return success;
}

void* worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    char password[PASSWORD_LENGTH + 1];
    password[PASSWORD_LENGTH] = '\0';
    int err = 0;
    zip_t* za = zip_open(data->zip_path, 0, &err);
    if (!za) {
        fprintf(stderr, "线程 %d 无法打开ZIP文件\n", data->thread_id);
        return NULL;
    }
    for (int i = data->start; i <= data->end; i++) {
        if (found) break; // 发现密码就立即退出

        snprintf(password, PASSWORD_LENGTH + 1, "%06d", i);
        if (try_password(za, password)) {
            pthread_mutex_lock(&lock);
            if (!found) {
                found = 1;
                strcpy(found_password, password);
            }
            pthread_mutex_unlock(&lock);
            break;
        }
    }
    zip_close(za);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("用法: %s zip文件路径\n", argv[0]);
        return 1;
    }
    const char* zip_path = argv[1];
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count <= 0) cpu_count = 4;
    int thread_count = cpu_count;
    pthread_t threads[thread_count];
    ThreadData thread_data[thread_count];
    pthread_mutex_init(&lock, NULL);
    int range_per_thread = 1000000 / thread_count;
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].zip_path = zip_path;
        thread_data[i].start = i * range_per_thread;
        thread_data[i].end = (i == thread_count - 1) ? 999999 : (thread_data[i].start + range_per_thread - 1);
        thread_data[i].thread_id = i + 1;
        pthread_create(&threads[i], NULL, worker, &thread_data[i]);
    }
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    if (found) {
        printf("%s\n", found_password);
        return 0;
    } else {
        fprintf(stderr, "未找到密码\n");
        return 2;
    }
}
