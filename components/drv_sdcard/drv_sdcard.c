#include "drv_sdcard.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "esp_log.h"                   // 确保有这个头
#include <errno.h>
static const char *TAG = "DRV_SDCARD"; // <<==== 新增这一行

static sdmmc_card_t *s_card = NULL;

esp_err_t drv_sdcard_init(void)
{
    esp_err_t ret;
    // ========== 新增：释放USB D-(IO19) D+(IO20)，关闭USB PHY占用 ==========
    gpio_config_t usb_pins_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_19) | (1ULL << GPIO_NUM_20),
        .mode = GPIO_MODE_INPUT,           // 先设为输入，SDMMC会后续接管
        .pull_up_en = GPIO_PULLUP_DISABLE, // 你外部已经上拉，内部禁止上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&usb_pins_conf);
    // =====================================================================

    // SDMMC主机配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 4000; // 初始化阶段低速4MHz
    // SD卡槽配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = GPIO_NUM_47;
    slot_config.cmd = GPIO_NUM_48;
    slot_config.d0 = GPIO_NUM_19;
    slot_config.width = 1; // 1线模式，只用DAT0
    // slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 关闭内部弱上拉，外部已有上拉

    // FATFS挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    // 挂载SD卡到 /sd 路径
    ret = esp_vfs_fat_sdmmc_mount("/sd", &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            printf("Failed to mount filesystem. Card cannot be mounted.\n");
        }
        else
        {
            printf("Failed to initialize the card (%s).\n", esp_err_to_name(ret));
        }
        return ret;
    }
    printf("SD card mounted success\n");
    sdmmc_card_print_info(stdout, s_card);

    // // ===================== SD卡读写测试代码 开始 =====================
    // printf("\r\n==== SD Card Write Test ====\r\n");
    // int fd = open("/sd/test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // if (fd >= 0)
    // {
    //     const char *str = "Hello SD Card, ESP32S3 test.\n";
    //     ssize_t wlen = write(fd, str, strlen(str));
    //     if (wlen > 0)
    //     {
    //         printf("Write OK, wrote %d bytes\r\n", (int)wlen);
    //     }
    //     else
    //     {
    //         printf("Write fail!\r\n");
    //     }
    //     close(fd);
    // }
    // else
    // {
    //     printf("Open /sd/test.txt failed, fd=%d\r\n", fd);
    // }

    // printf("\r\n==== SD Card Read Test ====\r\n");
    // fd = open("/sd/test.txt", O_RDONLY);
    // if (fd >= 0)
    // {
    //     char buf[64] = {0};
    //     ssize_t rlen = read(fd, buf, sizeof(buf) - 1);
    //     if (rlen > 0)
    //     {
    //         printf("Read content: %s\r\n", buf);
    //     }
    //     else
    //     {
    //         printf("Read 0 bytes\r\n");
    //     }
    //     close(fd);
    // }
    // else
    // {
    //     printf("Open read file failed\r\n");
    // }
    // // ===================== SD卡读写测试代码 结束 =====================

    return ESP_OK;
}

esp_err_t drv_sdcard_deinit(void)
{
    if (s_card)
    {
        esp_vfs_fat_sdcard_unmount("/sd", s_card);
        s_card = NULL;
    }
    return ESP_OK;
}

void drv_sdcard_append_csv(const char *line)
{
    const char *file_path = "/sd/log.csv";
    if (s_card == NULL)
    {
        printf("SD card not mounted!\n");
        return;
    }
    // O_APPEND 追加模式，每次写在文件末尾
    int fd = open(file_path, O_WRONLY | O_CREAT, 0777);
    if (fd < 0)
    {
        printf("open csv file failed, path=%s, errno=%d\n", file_path, errno);
        return;
    }
    // 判断是否是新文件，如果文件为空，先写入表头
    lseek(fd, 0, SEEK_END);
    struct stat stat_buf;
    fstat(fd, &stat_buf);
    if (stat_buf.st_size == 0)
    {
        // ===== 修改表头，增加CO2 =====
        const char *header = "Time,DeviceID,Addr,Temperature,Humidity,CO2\n";
        write(fd, header, strlen(header));
    }
    write(fd, line, strlen(line));
    close(fd);
}

void drv_test_write_csv(void)
{
    char csv_line[128];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    snprintf(csv_line, sizeof(csv_line), "%s,1,0xa9a6,31.06,56.29\n", time_str);
    drv_sdcard_append_csv(csv_line);
    ESP_LOGI(TAG, "【模拟测试】写入一行数据到csv: %s", csv_line);
}