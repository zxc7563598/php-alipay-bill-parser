#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zip.h>

#define PASSWORD_LENGTH 6
#define THREAD_COUNT 4

typedef struct {
    const char* zip_path;
    int start;
    int end;
    int thread_id;
} ThreadData;

volatile int found = 0;
char found_password[PASSWORD_LENGTH + 1];
pthread_mutex_t lock;

int try_password(const char* zip_path, const char* password) {
    int err = 0;
    zip_t* za = zip_open(zip_path, 0, &err);
    if (!za) return 0;
    if (zip_set_default_password(za, password) < 0) {
        zip_close(za);
        return 0;
    }
    zip_file_t* zf = zip_fopen_index(za, 0, 0);
    if (zf == NULL) {
        zip_close(za);
        return 0;
    }
    char buffer[4096];
    zip_int64_t bytes_read;
    int success = 1;
    while ((bytes_read = zip_fread(zf, buffer, sizeof(buffer))) > 0) {}
    if (bytes_read < 0) {
        success = 0;
    }
    zip_fclose(zf);
    zip_close(za);
    return success;
}

void* worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    char password[PASSWORD_LENGTH + 1];
    password[PASSWORD_LENGTH] = '\0';
    for (int i = data->start; i <= data->end && !found; i++) {
        // 生成密码字符串
        snprintf(password, PASSWORD_LENGTH + 1, "%06d", i);

        if (try_password(data->zip_path, password)) {
            pthread_mutex_lock(&lock);
            if (!found) {
                found = 1;
                strcpy(found_password, password);
            }
            pthread_mutex_unlock(&lock);
            break;
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("用法: %s zip文件路径\n", argv[0]);
        return 1;
    }
    const char* zip_path = argv[1];
    pthread_t threads[THREAD_COUNT];
    ThreadData thread_data[THREAD_COUNT];
    pthread_mutex_init(&lock, NULL);

    int range_per_thread = 1000000 / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].zip_path = zip_path;
        thread_data[i].start = i * range_per_thread;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? 999999 : (thread_data[i].start + range_per_thread - 1);
        thread_data[i].thread_id = i + 1;
        pthread_create(&threads[i], NULL, worker, &thread_data[i]);
    }
    for (int i = 0; i < THREAD_COUNT; i++) {
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
