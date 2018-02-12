#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "driver/i2s.h"
#include "app_main.h"

#include <errno.h>
#include <sys/fcntl.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "spiffs_vfs.h"

#define TAG "main"


static void list(char *path) {

    DIR *dir = NULL;
    struct dirent *ent;
    char type;
    char size[9];
    char tpath[255];
    char tbuffer[80];
    struct stat sb;
    struct tm *tm_info;
    char *lpath = NULL;
    int statok;

    printf("LIST of DIR [%s]\r\n", path);
    // Open directory
    dir = opendir(path);
    if (!dir) {
        printf("Error opening directory\r\n");
        return;
    }

    // Read directory entries
    uint64_t total = 0;
    int nfiles = 0;
    printf("T  Size      Date/Time         Name\r\n");
    printf("-----------------------------------\r\n");
    while ((ent = readdir(dir)) != NULL) {
        sprintf(tpath, path);
        if (path[strlen(path)-1] != '/') strcat(tpath,"/");
        strcat(tpath,ent->d_name);
        tbuffer[0] = '\0';


            // Get file stat
            statok = stat(tpath, &sb);

            if (statok == 0) {
                tm_info = localtime(&sb.st_mtime);
                strftime(tbuffer, 80, "%d/%m/%Y %R", tm_info);
            }
            else sprintf(tbuffer, "                ");

            if (ent->d_type == DT_REG) {
                type = 'f';
                nfiles++;
                if (statok) strcpy(size, "       ?");
                else {
                    total += sb.st_size;
                    if (sb.st_size < (1024*1024)) sprintf(size,"%8d", (int)sb.st_size);
                    else if ((sb.st_size/1024) < (1024*1024)) sprintf(size,"%6dKB", (int)(sb.st_size / 1024));
                    else sprintf(size,"%6dMB", (int)(sb.st_size / (1024 * 1024)));
                }
            }
            else {
                type = 'd';
                strcpy(size, "       -");
            }

            printf("%c  %s  %s  %s\r\n",
                type,
                size,
                tbuffer,
                ent->d_name
            );

    }
    if (total) {
        printf("-----------------------------------\r\n");
        if (total < (1024*1024)) printf("   %8d", (int)total);
        else if ((total/1024) < (1024*1024)) printf("   %6dKB", (int)(total / 1024));
        else printf("   %6dMB", (int)(total / (1024 * 1024)));
        printf(" in %d file(s)\r\n", nfiles);
    }
    printf("-----------------------------------\r\n");

    closedir(dir);

    free(lpath);

    uint32_t tot, used;
    spiffs_fs_stat(&tot, &used);
    printf("SPIFFS: free %d KB of %d KB\r\n", (tot-used) / 1024, tot / 1024);
}


void init_i2s()
{
    i2s_config_t i2s_config = {
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = 44100,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
            .dma_buf_count = 32,
            .dma_buf_len = 64,
            .use_apll = 0,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };

    i2s_pin_config_t pin_config =
    {
            .bck_io_num = GPIO_NUM_26,
            .ws_io_num = GPIO_NUM_25,
            .data_out_num = GPIO_NUM_22,
            .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    //i2s_stop(I2S_NUM_0);
}

static int play_file(char *fname)
{
    char buf[8192];
    ssize_t nread;
    int res;

    FILE *fd = fopen(fname, "rb");
    if (fd == NULL) {
        ESP_LOGE("[read]", "fopen failed");
        return -1;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_start(I2S_NUM_0);

    while (nread = fread(buf, 1, sizeof(buf), fd), nread > 0) {
        i2s_write_bytes(I2S_NUM_0, buf, nread, portMAX_DELAY);
    }

    // zero I2S buffer in a blocking way
//    memset(buf, 0, sizeof(buf));
//    i2s_write_bytes(I2S_NUM_0, buf, nread, portMAX_DELAY);
//    i2s_write_bytes(I2S_NUM_0, buf, nread, portMAX_DELAY);

    i2s_stop(I2S_NUM_0);

    res = fclose(fd);
    if (res) {
        ESP_LOGE("[read]", "fclose failed: %d", res);
        return -4;
    }

    return 0;
}

static int last_file_idx = 0;
void play_next_sound()
{
    char *path = "/spiffs/";
    struct dirent *ent;
    DIR *dir = NULL;
    char tpath[255];
    int curr_file_idx = 0;

    dir = opendir(path);
    if (!dir) {
        printf("Error opening directory %s\n", path);
        return;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        curr_file_idx++;

        if (ent->d_type != DT_REG) {
            continue;
        }

        if(curr_file_idx <= last_file_idx) {
            continue;
        } else {
            last_file_idx = curr_file_idx;
        }

        sprintf(tpath, path);
        if (path[strlen(path)-1] != '/')
            strcat(tpath,"/");
        strcat(tpath, ent->d_name);

        printf("playing %s\n", tpath);

        play_file(tpath);
        break;
    }

    if(last_file_idx > 9)
        last_file_idx = 0;

    closedir(dir);
}


#define ESP_INTR_FLAG_DEFAULT 0
typedef void (*asio_gpio_handler_t)(gpio_num_t io_num, void *user_data);
typedef struct {
    xQueueHandle gpio_evt_queue;
    gpio_num_t gpio_num;
    asio_gpio_handler_t callback;
} asio_gpio_context_t;



/* gpio event handler */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    asio_gpio_context_t *gpio_ctx = arg;

    xQueueSendToBackFromISR(gpio_ctx->gpio_evt_queue, &gpio_ctx->gpio_num, &xHigherPriorityTaskWoken);

    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


void init_gpio(asio_gpio_context_t *gpio_ctx)
{
    gpio_config_t io_conf;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO0 here ("Boot" button)
    io_conf.pin_bit_mask = (1 << gpio_ctx->gpio_num);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // remove existing handler that may be present
    gpio_isr_handler_remove(gpio_ctx->gpio_num);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(gpio_ctx->gpio_num, gpio_isr_handler, gpio_ctx);
}





static void gpio_callback(gpio_num_t io_num, void *user_data)
{
    play_next_sound();
}

/**
 * entry point
 */
void app_main()
{
    ESP_LOGI(TAG, "starting app_main()");
    ESP_LOGW(TAG, "%d: - RAM left %d", __LINE__, esp_get_free_heap_size());

    vfs_spiffs_register();
    list("/spiffs/");

    init_i2s();

    asio_gpio_context_t *gpio_ctx = calloc(1, sizeof(asio_gpio_context_t));
    gpio_ctx->callback = gpio_callback;
    gpio_ctx->gpio_num = GPIO_NUM_0;
    gpio_ctx->gpio_evt_queue = xQueueCreate(1, sizeof(gpio_num_t));

    init_gpio(gpio_ctx);

    uint32_t io_num;
    while(1)
    {
        if (xQueueReceive(gpio_ctx->gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
            gpio_ctx->callback(io_num, NULL);
        }
    }

    ESP_LOGW(TAG, "%d: - RAM left %d", __LINE__, esp_get_free_heap_size());
    vTaskDelete(NULL);
}
