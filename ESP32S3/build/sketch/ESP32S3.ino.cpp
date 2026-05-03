#line 1 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
#include <Arduino.h>
#include <driver/i2s.h>  // ESP-IDF 的 I2S 驱动接口（底层）
#include <soc/i2s_reg.h> // 访问 I2S 寄存器（本例未直接使用，但常用于高级调试）
#include <WiFi.h>        // 包含 WiFi 库（本例未使用，但常用于网络功能）
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// ====== 硬件引脚配置 ======
#define BUTTON_PIN 0 // 按键连接到 GPIO0（开发板上通常有 BOOT 按钮，注意避免干扰启动）

// ====== 服务器配置 ======
const char *ssid = "Mi";
const char *password = "3027Endid";
const char *serverUrl = "http://190.92.241.121:8788/upload";
// ===== 主题 =====
const char *sub_topic = "esp32/sub"; // 订阅的主题
const char *pub_topic = "esp32/pub"; // 发布的主题

// ===== MQTT 代理配置 =====
const char *mqtt_server = "190.92.241.121"; // 公共 MQTT 代理，也可用自建服务器
const int mqtt_port = 1883;                 // MQTT 默认端口
const char *mqtt_user = NULL;               // 如有用户名，填写
const char *mqtt_password = NULL;           // 如有密码，填写

// 麦克风 (INMP441) 使用 I2S0 接口（只读）
#define MIC_I2S I2S_NUM_0 // 使用 I2S 控制器 0
#define MIC_BCLK 5        // 位时钟（Bit Clock）
#define MIC_WS 6          // 字选择（Word Select / LRCLK）
#define MIC_DATA 4        // 数据输入（麦克风输出到 ESP32）

// 扬声器 (MAX98357A) 使用 I2S1 接口（只写）
#define SPK_I2S I2S_NUM_1 // 使用 I2S 控制器 1（ESP32 支持双 I2S）
#define SPK_BCLK 14       // 位时钟输出
#define SPK_WS 21         // 字选择输出
#define SPK_DATA 13       // 数据输出（ESP32 输出到功放）

// ====== 音频参数配置 ======
#define SAMPLE_RATE 16000                           // 采样率：16kHz（语音常用）
#define SAMPLE_BITS 16                              // 位深：16-bit PCM
#define RECORD_TIME_SEC 5                           // 录音时长（秒）
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_TIME_SEC) // 总样本数（单声道）
#define LedPin 1                                    // LED 引脚

// ====== 全局变量 ======
// 使用 uint32_t* 指针，但实际存储的是 int16_t 样本（每个样本占 2 字节）
// 之所以用 uint32_t 是为了确保内存对齐（ESP32 DMA 要求 4 字节对齐）
uint32_t *recordBuffer = nullptr;
size_t recordedSamples = 0;      // 实际录制的样本数量
bool isRecording = false;        // 录音状态标志
unsigned long lastCheckTime = 0; // 上次音量检测时间
int sound_threshold = 4000;      // 音量阈值
bool isFirstLoop = true;         // 首次运行标志
const char *audioUrl = "";

HTTPClient http;
WiFiClient espClient;
PubSubClient client(espClient);
JsonDocument updoc;
JsonDocument downdoc;

// 0表示未操作，1表示开，3表示关
struct Data
{
    int temperature = 3;
    int humidity = 3;
    int co2 = 3;
    float co = 3;
    float NH3 = 3;
    int air_quality_index = 3;
    String air_quality = "";
    int sun = 3;
    int device_pump = 3;
    int device_humidity = 3;
    int device_fan = 3;
    int device_servo = 3;
    // int device_feeder = 0;
    int device_light = 3;
};
typedef struct Data data;
data sensorData;

// 收到订阅消息时的回调函数
// void callback(char *topic, byte *payload, unsigned int length)
// {
//     Serial.print("Message arrived [");
//     Serial.print(topic);
//     Serial.print("] ");

//     // 将 payload 转换为字符串并打印
//     String messageTemp;
//     for (unsigned int i = 0; i < length; i++)
//     {
//         messageTemp += (char)payload[i];
//     }
//     Serial.println(messageTemp);

//     // 根据消息内容执行动作（示例：如果消息是 "on"，则……）
//     if (String(topic) == sub_topic)
//     {
//         if (messageTemp == "on")
//         {
//             // 执行开灯等操作
//             Serial.println("Received ON command");
//         }
//         else if (messageTemp == "off")
//         {
//             // 执行关灯等操作
//             Serial.println("Received OFF command");
//         }
//     }
// }



// 收到订阅消息时的回调函数 【修复版】
// void callback(char *topic, byte *payload, unsigned int length)
// {
//     Serial.print("Message arrived [");
//     Serial.print(topic);
//     Serial.print("] ");

//     // 把收到的消息转成字符串
//     String messageTemp;
//     for (unsigned int i = 0; i < length; i++) {
//         messageTemp += (char)payload[i];
//     }
//     Serial.println(messageTemp);

//     // 如果是订阅的主题 esp32/sub
//     if (String(topic) == sub_topic)
//     {
//         // 直接把收到的指令传给控制函数！
//         mission_update(messageTemp.c_str());
//     }
// }




// 收到 esp32/sub 消息时 → 自动解析 JSON → 控制设备
#line 142 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void callback(char *topic, byte *payload, unsigned int length);
#line 188 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void reconnect();
#line 237 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void PublishMessage();
#line 300 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void PublishServo(int data);
#line 333 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
bool initMicI2S();
#line 372 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
bool initSpkI2S();
#line 403 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void recordAudio();
#line 453 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
int calculateSoundLevel(int16_t *data, size_t count);
#line 472 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void playAudioFromURL(const char *url);
#line 558 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
bool uploadAudio(uint8_t *audio_data, int data_size);
#line 604 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
int getAudioLevel();
#line 641 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void servopulse(int angle);
#line 658 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void mission_init();
#line 667 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void mission_action();
#line 746 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void mission_update(const char *text);
#line 807 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void recordBuffer_malloc_init();
#line 824 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void init_I2S();
#line 840 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void wifi_init();
#line 854 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void setup();
#line 875 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void loop();
#line 142 "F:\\code\\vs\\Graduation\\ESP32S3\\ESP32S3.ino"
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    // 1. 先把字节数组转成字符串
    String jsonStr;
    for (unsigned int i = 0; i < length; i++) {
        jsonStr += (char)payload[i];
    }
    Serial.println(jsonStr);

    // 2. 只处理 esp32/sub 主题
    if (String(topic) != sub_topic) return;

    // 3. 解析 JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error) {
        Serial.print("JSON 解析失败: ");
        Serial.println(error.c_str());
        return;
    }

    // 4. 提取 devices 对象
    JsonObject devices = doc["devices"];
    if (!devices) {
        Serial.println("未找到 devices 字段");
        return;
    }

    // 【核心修复】检查键名是否存在，存在才更新！
    if (devices.containsKey("light"))           sensorData.device_light = devices["light"];
    if (devices.containsKey("pump"))            sensorData.device_pump  = devices["pump"];
    if (devices.containsKey("fan"))             sensorData.device_fan   = devices["fan"];
    if (devices.containsKey("servo"))           sensorData.device_servo = devices["servo"];
    if (devices.containsKey("device_humidity")) sensorData.device_humidity = devices["device_humidity"];

    // 7. 立即执行动作
    mission_action();
}


// 重新连接 MQTT 代理
void reconnect()
{
    // 循环直到重新连接成功
    while (!client.connected())
    {
        Serial.print("🔄 尝试连接 MQTT...");

        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);

        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password))
        {
            Serial.println("✅ 连接成功");
            client.subscribe(sub_topic);
            Serial.print("📬 已订阅：");
            Serial.println(sub_topic);
        }
        else
        {
            Serial.print("❌ 失败，rc=");
            Serial.print(client.state());
            Serial.println(" 5 秒后重试");

            // 显示常见错误码含义
            if (client.state() == -4)
                Serial.println("  → 连接超时");
            if (client.state() == -3)
                Serial.println("  → 连接丢失");
            if (client.state() == -2)
                Serial.println("  → 连接失败");
            if (client.state() == -1)
                Serial.println("  → 未连接");
            if (client.state() == 1)
                Serial.println("  → 协议错误");
            if (client.state() == 2)
                Serial.println("  → 客户端 ID 无效");
            if (client.state() == 3)
                Serial.println("  → 服务器不可用");
            if (client.state() == 4)
                Serial.println("  → 用户名/密码错误");
            if (client.state() == 5)
                Serial.println("  → 未授权");

            delay(5000);
        }
    }
}

// 发布消息
void PublishMessage()
{

    // ✅ 检查 MQTT 连接状态
    if (!client.connected())
    {
        Serial.println("MQTT 未连接，跳过发布");
        return;
    }

    // ✅ 增大缓冲区到 128 字节
    char jsonBuffer[128];
    updoc.clear();
    // updoc[ID] = data;
    updoc["time"] = millis();
    updoc["pump"] = sensorData.device_pump;
    updoc["device_humidity"] = sensorData.device_humidity;
    updoc["fan"] = sensorData.device_fan;
    updoc["servo"] = sensorData.device_servo;
    updoc["light"] = sensorData.device_light;

    size_t len = serializeJson(updoc, jsonBuffer);

    Serial.printf("准备发布：%s (长度：%d)\n", jsonBuffer, len);

    if (client.publish(pub_topic, jsonBuffer))
    {
        Serial.print("✅ 发布成功：");
        Serial.println(jsonBuffer);
    }
    else
    {
        Serial.println("❌ 发布失败");
        Serial.printf("MQTT 状态码：%d\n", client.state());
    }

    // 在ESP32那里

    // // String message = "";
    // char jsonBuffer[512];
    // updoc.clear();
    // updoc["temperature"] = sensorData.temperature;
    // updoc["humidity"] = sensorData.humidity;
    // updoc["co2"] = sensorData.co2;
    // updoc["co"] = sensorData.co;
    // updoc["NH3"] = sensorData.NH3;
    // updoc["sun"] = sensorData.sun;

    // // updoc["air_quality_index"] = sensorData.air_quality_index;
    // size_t len = serializeJson(updoc, jsonBuffer);

    // if (client.publish(pub_topic, jsonBuffer))
    // {
    //     Serial.print("Message published: ");
    //     Serial.print("jsonBuffer: " + String(jsonBuffer) + "\n");
    //     Serial.print("len:" + String(len) + "\n");
    // }
    // else
    // {
    //     Serial.println("Failed to publish message");
    // }
}

void PublishServo(int data)
{

    // ✅ 检查 MQTT 连接状态
    if (!client.connected())
    {
        Serial.println("MQTT 未连接，跳过发布");
        return;
    }

    // ✅ 增大缓冲区到 128 字节
    char jsonBuffer[32];
    updoc.clear();
    // updoc[ID] = data;
    updoc["time"] = millis();
    updoc["servo"] = data;

    size_t len = serializeJson(updoc, jsonBuffer);

    Serial.printf("准备发布：%s (长度：%d)\n", jsonBuffer, len);

    if (client.publish(pub_topic, jsonBuffer))
    {
        Serial.print("✅ 发布成功：");
        Serial.println(jsonBuffer);
    }
    else
    {
        Serial.println("❌ 发布失败");
        Serial.printf("MQTT 状态码：%d\n", client.state());
    }
}
// 初始化麦克风 I2S（仅接收模式）
bool initMicI2S()
{
    // 配置 I2S 参数
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 主机 + 接收模式
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,                            // 单声道（左声道）
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S), // 标准 I2S 协议
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                               // 中断等级
        .dma_buf_count = 8,                                                     // DMA 缓冲区数量
        .dma_buf_len = 128,                                                     // 每个缓冲区长度（样本数）
        .use_apll = false,                                                      // 不使用音频 PLL（节省功耗）
        .tx_desc_auto_clear = false,                                            // 仅用于 TX，此处无效
        .fixed_mclk = 0                                                         // 自动计算 MCLK
    };

    // 配置引脚映射
    i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_BCLK,
        .ws_io_num = MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE, // 不使用输出
        .data_in_num = MIC_DATA            // 数据从 MIC 输入
    };

    // 安装 I2S 驱动
    esp_err_t err = i2s_driver_install(MIC_I2S, &i2s_config, 0, NULL);
    if (err != ESP_OK)
        return false;

    // 设置引脚
    err = i2s_set_pin(MIC_I2S, &pin_config);
    if (err != ESP_OK)
        return false;

    return true;
}

// 初始化扬声器 I2S（仅发送模式）
bool initSpkI2S()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // 主机 + 发送模式
        .sample_rate = 24000,
        .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = true, // 发送完成后自动清空描述符（推荐）
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = SPK_BCLK,
        .ws_io_num = SPK_WS,
        .data_out_num = SPK_DATA, // 数据输出到扬声器
        .data_in_num = I2S_PIN_NO_CHANGE};

    esp_err_t err = i2s_driver_install(SPK_I2S, &i2s_config, 0, NULL);
    if (err != ESP_OK)
        return false;
    err = i2s_set_pin(SPK_I2S, &pin_config);
    if (err != ESP_OK)
        return false;
    return true;
}

// 录音函数：从麦克风读取音频数据
void recordAudio()
{
    Serial.println("开始录音 5 秒...");

    // if (!initMicI2S())
    // {
    //     Serial.println("麦克风 I2S 初始化失败");
    //     return;
    // }

    // 计算需读取的总字节数（int16_t × 样本数）
    size_t bytesToRead = BUFFER_SIZE * sizeof(int16_t);
    size_t bytesRead = 0;

    // 阻塞式读取：直到读满或超时（portMAX_DELAY = 永不超时）
    i2s_read(MIC_I2S, recordBuffer, bytesToRead, &bytesRead, portMAX_DELAY);

    // 计算实际读取的样本数（每个样本 2 字节）
    recordedSamples = bytesRead / sizeof(int16_t);

    // 卸载驱动释放资源
    // i2s_driver_uninstall(MIC_I2S);
    Serial.printf("录音完成，共 %d 个样本\n", recordedSamples);
}

// 播放函数：将缓冲区数据输出到扬声器
// void playAudio()
// {
//     Serial.println("开始播放...");

//     // if (!initSpkI2S())
//     // {
//     //     Serial.println("扬声器 I2S 初始化失败");
//     //     return;
//     // }

//     size_t bytesToWrite = recordedSamples * sizeof(int16_t);
//     size_t bytesWritten = 0;

//     // 阻塞式写入音频数据
//     i2s_write(SPK_I2S, recordBuffer, bytesToWrite, &bytesWritten, portMAX_DELAY);

//     // i2s_driver_uninstall(SPK_I2S);
//     Serial.println("播放完成");
// }

/**
 * 计算声音强度 (RMS - 均方根)
 * 返回值为 0-32767 之间的整数，代表当前音量大小
 */
int calculateSoundLevel(int16_t *data, size_t count)
{
    if (count == 0)
        return 0;

    float sum = 0;
    for (size_t i = 0; i < count; i++)
    {
        // 累加平方值
        sum += (float)data[i] * (float)data[i];
    }

    // 计算均方根
    float rms = sqrt(sum / count);

    // 转换为整数返回
    return (int)rms;
}

void playAudioFromURL(const char *url)
{
    Serial.println("开始从网络播放 WAV...");

    // // 初始化扬声器 I2S（如果尚未初始化）
    // if (!initSpkI2S())
    // {
    //     Serial.println("扬声器 I2S 初始化失败");
    //     return;
    // }

    // HTTPClient http;
    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode != HTTP_CODE_OK)
    {
        Serial.printf("HTTP 请求失败，状态码：%d\n", httpResponseCode);
        http.end();
        // i2s_driver_uninstall(SPK_I2S);
        return;
    }

    // 获取响应流
    WiFiClient *stream = http.getStreamPtr();
    if (!stream)
    {
        Serial.println("无法获取 HTTP 流");
        http.end();
        // i2s_driver_uninstall(SPK_I2S);
        return;
    }

    // 跳过 WAV 头部（标准 PCM WAV 头为 44 字节）
    const size_t WAV_HEADER_SIZE = 44;
    uint8_t header[WAV_HEADER_SIZE];
    size_t readHeader = stream->readBytes(header, WAV_HEADER_SIZE);
    if (readHeader != WAV_HEADER_SIZE)
    {
        Serial.println("WAV 头读取不完整，可能不是标准 WAV 文件");
        http.end();
        // i2s_driver_uninstall(SPK_I2S);
        return;
    }

    Serial.println("WAV 头已跳过，开始播放 PCM 数据...");

    // 播放缓冲区（建议 1024~4096 字节）
    const size_t PLAY_BUFFER_SIZE = 4096;
    uint8_t playBuffer[PLAY_BUFFER_SIZE];

    size_t totalBytesPlayed = 0;

    // 修复：移除 http.available()，只使用 stream->available()
    while (http.connected() && stream->available() > 0)
    {
        // 从网络流读取数据
        size_t bytesRead = stream->readBytes(playBuffer, PLAY_BUFFER_SIZE);
        if (bytesRead == 0)
        {
            delay(1); // 避免忙等
            continue;
        }

        // 将 PCM 数据写入 I2S（阻塞式）
        size_t bytesWritten = 0;
        esp_err_t err = i2s_write(SPK_I2S, playBuffer, bytesRead, &bytesWritten, pdMS_TO_TICKS(200));
        if (err != ESP_OK)
        {
            Serial.println("I2S 写入错误");
            break;
        }

        totalBytesPlayed += bytesWritten;
    }

    // 等待 I2S FIFO 清空（可选，确保声音播完）
    i2s_zero_dma_buffer(SPK_I2S);

    http.end();
    // i2s_driver_uninstall(SPK_I2S);

    Serial.printf("播放完成，共播放 %d 字节 PCM 数据\n", totalBytesPlayed);
}

// 上传音频数据到服务器
bool uploadAudio(uint8_t *audio_data, int data_size)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi未连接，无法上传");
        return false;
    }

    // HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/octet-stream"); // 用于PCM数据

    int httpResponseCode = http.POST(audio_data, data_size);

    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.printf("上传成功，响应码: %d\n", httpResponseCode);
        Serial.println(response);
        http.end();
        // audioUrl = response.audio_url; // 假设服务器返回的是音频文件的 URL

        JsonDocument doc;
        deserializeJson(doc, response);
        mission_update(doc["intent"].as<const char *>());
        audioUrl = doc["audio_url"].as<const char *>();
        Serial.println(audioUrl);
        if (audioUrl && strlen(audioUrl) > 20)
        {
            playAudioFromURL(audioUrl);
        }
        // playAudioFromURL("http://192.168.10.5:8788/audio_assistant_response.wav");
        // playAudioFromURL("http://dashscope-result-bj.oss-cn-beijing.aliyuncs.com/prod/qwen3-tts/20260311/1860753901094698/1678fdbd-ed32-4452-8f0d-5c61cc35a3fb.wav?Expires=1773255776&OSSAccessKeyId=LTAI5tGzqbGcEmE58b221XQy&Signature=9v8NFq8dOSK44Py9SABNrd7R6To%3D");

        return true;
    }
    else
    {
        Serial.printf("上传失败，响应码: %d\n", httpResponseCode);
        http.end();
        return false;
    }
}

// 获取当前音频的音量水平
// 获取当前音频的音量水平
int getAudioLevel()
{
    // 使用小缓冲区（64 个样本），避免栈溢出
    int16_t samples[64];
    size_t bytes_read = 0;

    // 非阻塞读取（0 毫秒超时）
    esp_err_t err = i2s_read(MIC_I2S, samples, sizeof(samples), &bytes_read, 0);

    if (err != ESP_OK || bytes_read == 0)
    {
        return 0;
    }

    // 计算平均绝对值作为音量水平
    long sum = 0;
    int sample_count = bytes_read / sizeof(int16_t);

    if (sample_count == 0)
        return 0;

    for (int i = 0; i < sample_count; i++)
    {
        sum += abs(samples[i]);
    }

    return sum / sample_count;
}

/**************************************灯 风扇 水泵 加湿器 舵机等********************************************************/

#define ledPin 12
#define pumpPin 11
#define humidityPin 10
#define fanPin 9
#define servoPin 46

void servopulse(int angle)
{
    // 根据角度计算高电平脉冲宽度 (单位：微秒)
    // 0 度时脉宽约 500us，180 度时脉宽约 2480us
    int pulsewidth = (angle * 11) + 500;

    // 输出高电平，开始脉冲
    digitalWrite(servoPin, HIGH);
    // 保持高电平指定时长
    delayMicroseconds(pulsewidth);

    // 输出低电平，结束脉冲
    digitalWrite(servoPin, LOW);
    // 保持低电平至完整周期 20ms
    delayMicroseconds(20000 - pulsewidth);
}

void mission_init()
{
    pinMode(ledPin, OUTPUT);
    pinMode(pumpPin, OUTPUT);
    pinMode(humidityPin, OUTPUT);
    pinMode(fanPin, OUTPUT);
    pinMode(servoPin, OUTPUT);
}

void mission_action()
{

    if (sensorData.device_light == 1)
    {
        digitalWrite(ledPin, HIGH);
        Serial.println("灯已开启");
    }
    if (sensorData.device_light == 0)
    {
        digitalWrite(ledPin, LOW);
        Serial.println("灯已关闭");
    }

    if (sensorData.device_pump == 1)
    {
        digitalWrite(pumpPin, HIGH);
        Serial.println("水泵已开启");
    }
    if (sensorData.device_pump == 0)
    {
        digitalWrite(pumpPin, LOW);
        Serial.println("水泵已关闭");
    }

    if (sensorData.device_humidity == 1)
    {
        digitalWrite(humidityPin, HIGH);
        Serial.println("加湿器已开启");
    }
    if (sensorData.device_humidity == 0)
    {
        digitalWrite(humidityPin, LOW);
        Serial.println("加湿器已关闭");
    }

    if (sensorData.device_fan == 1)
    {
        digitalWrite(fanPin, HIGH);
        Serial.println("风扇已开启");
    }
    if (sensorData.device_fan == 0)
    {
        digitalWrite(fanPin, LOW);
        Serial.println("风扇已关闭");
    }

    if (sensorData.device_servo == 1)
    {
        for (int angle = 0; angle < 60; angle += 2) // 角度从 0 递增到 180，步长 2 度
        {
            // 每个角度重复发送 5 次脉冲，确保舵机稳定到达目标位置
            for (int i = 0; i < 3; i++)
            {
                servopulse(angle);
            }
        }
        PublishServo(1);
        sensorData.device_servo = 3;
        Serial.println("舵机已开启");
    }
    if (sensorData.device_servo == 2)
    {
        for (int angle = 60; angle > 0; angle -= 2) // 角度从 180 递减到 0，步长 2 度
        {
            // 每个角度重复发送 5 次脉冲，确保舵机稳定到达目标位置
            for (int i = 0; i < 3; i++)
            {
                servopulse(angle);
            }
        }
        PublishServo(2);
        sensorData.device_servo = 3;
        Serial.println("舵机已关闭");
    }
    PublishMessage();
    delay(1000);
}

void mission_update(const char *text)
{
    if (strstr(text, "open_fan"))
    {
        Serial.println("open_fan");
        sensorData.device_fan = 1;
    }
    else if (strstr(text, "close_fan"))
    {
        Serial.println("close_fan");
        sensorData.device_fan = 0;
    }
    else if (strstr(text, "open_light"))
    {
        Serial.println("open_light");
        sensorData.device_light = 1;
    }
    else if (strstr(text, "close_light"))
    {
        Serial.println("close_light");
        sensorData.device_light = 0;
    }
    else if (strstr(text, "open_pump"))
    {
        Serial.println("open_pump");
        sensorData.device_pump = 1;
    }
    else if (strstr(text, "close_pump"))
    {
        Serial.println("close_pump");
        sensorData.device_pump = 0;
    }
    else if (strstr(text, "open_humidity"))
    {
        Serial.println("open_humidity");
        sensorData.device_humidity = 1;
    }
    else if (strstr(text, "close_humidity"))
    {
        Serial.println("close_humidity");
        sensorData.device_humidity = 0;
    }
    else if (strstr(text, "open_feeder"))
    {
        Serial.println("open_feeder");
        sensorData.device_servo = 1;
    }
    else if (strstr(text, "close_feeder"))
    {
        Serial.println("close_feeder");
        sensorData.device_servo = 2;
    }
    else
    {
        Serial.println("no_intention");
    }
    mission_action();
}

/*****************************************************************************************************/

void recordBuffer_malloc_init()
{
    // 分配录音缓冲区：
    // - 总字节数 = 样本数 × 每样本字节数（16-bit = 2 字节）
    // - 使用 heap_caps_malloc 并指定 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    //   表示优先从 PSRAM（外部 RAM）分配，若无则从内部 RAM 分配 8 位对齐内存
    recordBuffer = (uint32_t *)heap_caps_malloc(BUFFER_SIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!recordBuffer)
    {
        Serial.println("内存分配失败！"); // 若设备无 PSRAM 或内存不足会失败
        while (1)
            delay(1000); // 死循环防止后续操作崩溃
    }
    Serial.println("录音缓冲区分配成功");
    // Serial.println("等待按键开始录音...");
}

void init_I2S()
{
    // 配置 I2S 接口

    if (!initSpkI2S())
    {
        Serial.println("扬声器 I2S 初始化失败");
        return;
    }
    if (!initMicI2S())
    {
        Serial.println("麦克风 I2S 初始化失败");
        return;
    }
}

void wifi_init()
{
    WiFi.begin(ssid, password);
    Serial.print("正在连接WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi连接成功");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
}

void setup()
{
    Serial.begin(115200); // 初始化串口用于调试输出
    // pinMode(BUTTON_PIN, INPUT_PULLUP); // 按键使用内部上拉（按下为 LOW）
    pinMode(LedPin, OUTPUT); // 初始化 LED 引脚为输出
    mission_init();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback); 
    client.setBufferSize(1024); // 设置缓冲区大小为 1024 字节
    http.setTimeout(8000); // 设置超时时间为 10 秒（单位毫秒），默认可能是 5 秒
    // 连接WiFi

    wifi_init();
    recordBuffer_malloc_init();
    init_I2S();

    // playAudioFromURL("http://192.168.10.5:8788/audio_assistant_response.wav");
    delay(1000); // 等待系统稳定
}

// 主循环：检测按键触发录音+播放
void loop()
{

    // int level = getAudioLevel();
    // Serial.print("音量：" + String(level) + "  ");

    // 首次进入 loop 时，初始化时间并跳过检测
    if (isFirstLoop)
    {
        // int level = getAudioLevel();
        // 欢迎语音 占位
        playAudioFromURL("http://190.92.241.121:8788/start.wav");
        isFirstLoop = false;
        delay(10);
        digitalWrite(LedPin, HIGH);
        return;
    }

    // ✅ 每次循环检查 MQTT 连接
    if (!client.connected())
    {
        reconnect();
    }
    client.loop(); // ✅ 保持 MQTT 心跳

    // --- 阶段 1: 监听模式 (检测声音阈值) ---
    // 我们每次读取一小段数据来计算音量，不存入最终缓冲区
    int16_t tempSampleBuffer[256]; // 临时小缓冲区
    size_t bytesRead = 0;

    // 读取一小块数据用于检测
    i2s_read(I2S_NUM_0, tempSampleBuffer, sizeof(tempSampleBuffer), &bytesRead, portMAX_DELAY);
    int sampleCount = bytesRead / 2; // 16bit = 2 bytes
    int level = calculateSoundLevel(tempSampleBuffer, sampleCount);

    if (millis() - lastCheckTime > 100 && !isRecording)
    {
        lastCheckTime = millis();

        // int level = getAudioLevel();
        // Serial.print("音量：" + String(level) + "  ");
        if (level > sound_threshold)
        {
            digitalWrite(LedPin, LOW);
            Serial.println("录音开始，音量水平：" + String(level));
            // sound_threshold = 2000;
            isRecording = true;
            recordAudio();
            uploadAudio((uint8_t *)recordBuffer, recordedSamples * sizeof(int16_t));
            // playAudio();

            isRecording = false;
            Serial.println("等待下一次触发...");

            // 录音后延时，避免连续触发
            delay(20);
            digitalWrite(LedPin, HIGH);
        }
        mission_action();
    }

    // if (millis() - lastCheckTime > 100 && !isRecording)
    // {
    //     lastCheckTime = millis();

    //     int level = getAudioLevel();
    //     // Serial.print("音量：" + String(level) + "  ");
    //     if (level > sound_threshold)
    //     {
    //         pinMode(LedPin, LOW);
    //         Serial.println("录音开始，音量水平：" + String(level));
    //         // sound_threshold = 2000;
    //         isRecording = true;
    //         recordAudio();
    //         // uploadAudio((uint8_t *)recordBuffer, recordedSamples * sizeof(int16_t));
    //         // playAudio();

    //         isRecording = false;
    //         Serial.println("等待下一次触发...");

    //         // 录音后延时，避免连续触发
    //         delay(200);
    //         pinMode(LedPin, HIGH);
    //     }
    // }

    delay(10);

    // if (digitalRead(BUTTON_PIN) == LOW) // 按键按下（低电平有效）
    // {
    //     delay(50);                          // 软件去抖（简单延时滤除机械抖动）
    //     if (digitalRead(BUTTON_PIN) == LOW) // 再次确认仍为按下
    //     {
    //         recordAudio(); // 录音
    //         playAudio();   // 播放刚录的内容
    //         Serial.println("等待下一次按键...");

    //         // 等待用户松开按键（避免重复触发）
    //         while (digitalRead(BUTTON_PIN) == LOW)
    //             ;
    //     }
    // }
    // delay(10); // 主循环小延时，降低 CPU 占用
}
