#line 1 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"

#include <Arduino.h>
#include <DHT11.h>
#include <MQUnifiedsensor.h>
#include <Wire.h>
#include "Adafruit_SGP30.h"
#include "MQ7.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ===== WiFi 配置 =====
const char *ssid = "Mi";         
const char *password = "3027Endid";

// ===== MQTT 代理配置 =====
const char *mqtt_server = "190.92.241.121"; // 公共 MQTT 代理，也可用自建服务器
const int mqtt_port = 1883;                 // MQTT 默认端口
const char *mqtt_user = NULL;               // 如有用户名，填写
const char *mqtt_password = NULL;           // 如有密码，填写

// ===== 主题 =====
const char *sub_topic = "esp32/sub"; // 订阅的主题
const char *pub_topic = "esp32/pub"; // 发布的主题

WiFiClient espClient;
PubSubClient client(espClient);

// 用于记录上次发布的时间（毫秒）
unsigned long lastMsg = 0;
const long interval = 500; // 每隔 2 秒发布一次



/**************************************data*******************************************************/
struct Data
{
    int temperature = 0;
    int humidity = 0;
    int co2 = 0;
    float co = 0;
    float NH3 = 0;
    int air_quality_index = 0;
    String air_quality = "";
    int sun = 0;
    int pump = 0;
    int fan = 0;
    int servo = 0;
};
typedef struct Data data;
data sensorData;

int counter = 0; // 计数器

JsonDocument updoc; // 用于存储要发布的 JSON 数据


// 连接 WiFi
#line 59 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void setup_wifi();
#line 81 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void callback(char *topic, byte *payload, unsigned int length);
#line 112 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void reconnect();
#line 142 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void PublishMessage();
#line 173 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void DHT11_Read();
#line 201 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void SGP30_Init();
#line 216 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void SGP30_Read();
#line 262 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void MQ7_init();
#line 269 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void MQ7_READ();
#line 289 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void Light_init();
#line 295 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void Light_read();
#line 318 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void MQ135_Init();
#line 335 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void MQ135_read();
#line 373 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
String interpret_air_quality(int sensor_value);
#line 388 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
float readSensorResistance();
#line 408 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
float calibrateR0();
#line 431 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void mission_init();
#line 440 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void mission_action();
#line 493 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void printData();
#line 523 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void setup();
#line 536 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void loop();
#line 59 "F:\\code\\vs\\Graduation\\ESP32\\ESP32.ino"
void setup_wifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// 收到订阅消息时的回调函数
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    // 将 payload 转换为字符串并打印
    String messageTemp;
    for (unsigned int i = 0; i < length; i++)
    {
        messageTemp += (char)payload[i];
    }
    Serial.println(messageTemp);

    // 根据消息内容执行动作（示例：如果消息是 "on"，则……）
    if (String(topic) == sub_topic)
    {
        if (messageTemp == "on")
        {
            // 执行开灯等操作
            Serial.println("Received ON command");
        }
        else if (messageTemp == "off")
        {
            // 执行关灯等操作
            Serial.println("Received OFF command");
        }
    }
}

// 重新连接 MQTT 代理
void reconnect()
{
    // 循环直到重新连接成功
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // 创建客户端 ID（通常需要唯一，这里使用随机数后缀）
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);

        // 尝试连接，可传入用户名和密码（如果代理需要认证）
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password))
        {
            Serial.println("connected");
            // 连接成功后订阅主题
            client.subscribe(sub_topic);
            Serial.print("Subscribed to: ");
            Serial.println(sub_topic);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

//发布消息
void PublishMessage()
{
    // String message = "";
    char jsonBuffer[512];
    updoc.clear();
    updoc["temperature"] = sensorData.temperature;
    updoc["humidity"] = sensorData.humidity;
    updoc["co2"] = sensorData.co2;
    updoc["co"] = sensorData.co;
    updoc["NH3"] = sensorData.NH3;
    updoc["sun"] = sensorData.sun;

    // updoc["air_quality_index"] = sensorData.air_quality_index;
    size_t len = serializeJson(updoc, jsonBuffer);

    if (client.publish(pub_topic, jsonBuffer))
    {   
        Serial.println("Message published");
        Serial.print("Message published: ");
        Serial.print("jsonBuffer: " + String(jsonBuffer)+"\n");
        Serial.print("len:"+String(len)+"\n");
    }
    else
    {
        Serial.println("Failed to publish message");
    }
}

/**************************************DHT11********************************************************/
#define DHT11_PIN 23
DHT11 dht11(DHT11_PIN);
void DHT11_Read()
{
    int temperature = 0;
    int humidity = 0;

    int result = dht11.readTemperatureHumidity(temperature, humidity);
    if (result == 0)
    {
        sensorData.temperature = temperature;
        sensorData.humidity = humidity;
        // Serial.print("温度: ");
        // Serial.print(temperature);
        // Serial.print("C\t湿度: ");
        // Serial.print(humidity);
        // Serial.println(" %");
    }
    else
    {
        Serial.println(DHT11::getErrorString(result));
    }
}
/*****************************************************************************************************/

/**************************************SGP30********************************************************/
Adafruit_SGP30 sgp;
#define SGP30_sda 15
#define SGP30_scl 2

void SGP30_Init()
{
    // 启动 I2C
    Wire.begin(SGP30_sda, SGP30_scl); // ESP32 默认使用 GPIO21(SDA) 和 GPIO22(SCL)

    if (!sgp.begin())
    {
        Serial.println("❌ 未找到 SGP30 传感器，请检查接线！");
    }
    else
        Serial.println("✅ SGP30 初始化成功！");

    sgp.IAQinit();
}

void SGP30_Read()
{
    // 读取一次数据（必须每秒调用一次以保持传感器内部状态）
    // 读取一次数据（必须每秒调用一次以保持传感器内部状态）
    if (!sgp.IAQmeasure())
    {
        // Serial.println("❌ SGP30 读取失败");

        return;
    }
    // 每60秒读取一次基线校准值（可选）
    counter++;
    if (counter == 5)
    {
        counter = 0;

        uint16_t TVOC_base, eCO2_base;
        if (!sgp.getIAQBaseline(&eCO2_base, &TVOC_base))
        {
            Serial.println("获取基线失败");
            return;
        }
    }

    // 打印结果
    sensorData.co2 = sgp.eCO2;
    // Serial.print("CO2: ");
    // Serial.print(sgp.eCO2); // 单位：ppm
    // Serial.print(" ppm\t");

    // Serial.print("TVOC: ");
    // Serial.print(sgp.TVOC); // 单位：ppb
    // Serial.println(" ppb");

    // 必须每秒调用一次 IAQmeasure()
    sgp.IAQmeasure();
}
/*****************************************************************************************************/

/**************************************MQ7********************************************************/
/* ===== MQ7 参数 ===== */
#define MQ7_PIN 33      // ESP32 ADC 引脚
#define MQ7_VOLTAGE 3.3 // MQ7 工作电压
MQ7 mq7(MQ7_PIN, MQ7_VOLTAGE);
unsigned long lastReadTime = 0;

void MQ7_init()
{
    Serial.println("MQ7 warming up & calibrating...");
    mq7.calibrate(); // 校准 R0
    Serial.println("MQ7 calibration finished");
}

void MQ7_READ()
{
    unsigned long now = millis();

    if (now - lastReadTime > 5000)
    { // 每 5 秒读取一次
        lastReadTime = now;

        float ppm = mq7.readPpm();
        sensorData.co = ppm;
        // Serial.print("CO Concentration: ");
        // Serial.print(ppm);
        // Serial.println(" ppm");
    }
}

/*****************************************************************************************************/

/*****************************************light sensor******************************************************/
#define lightPin 18
void Light_init()
{
    pinMode(lightPin, INPUT);
}

// 封装光敏传感器读取函数
void Light_read()
{
    sensorData.sun = digitalRead(lightPin);
}
/*****************************************************************************************************/

/**************************************MQ135********************************************************/
// ================= 硬件定义 =================
const int MQ135_PIN = 34;    // ESP32 Analog Pin
const float V_REF = 3.3;     // ESP32 ADC 参考电压
const int ADC_RES = 4095;    // ESP32 ADC 分辨率
const float RL_VALUE = 10.0; // 模块上的负载电阻 (通常10kΩ)

// ================= NH3 计算参数 (数据手册) =================
const float NH3_A = 102.2;
const float NH3_B = -2.473;

// ================= 通用空气质量参数 =================
int sensitivity = 200; // 用于计算通用质量分数的系数

// ================= 全局变量 =================
float R0 = 10.0; // 基准电阻，会在 setup 中自动校准

void MQ135_Init()
{
    pinMode(MQ135_PIN, INPUT);
    Serial.println("\n--------------------------------");
    Serial.println("MQ135 Sensor System Starting...");
    Serial.println("Pre-heating sensor (10 seconds)...");
    // 实际使用建议预热更久，这里为了演示设为10秒
    delay(10000);

    Serial.println("Calibrating R0 in current air...");
    R0 = calibrateR0();
    Serial.print("Calibration Done. Base Resistance (R0) = ");
    Serial.print(R0);
    Serial.println(" kOhms");
    Serial.println("--------------------------------\n");
}

void MQ135_read()
{

    // 1. 获取平滑后的电阻值 RS
    float rs = readSensorResistance();

    // ---------------- 功能一：计算氨气 PPM (科学计算) ----------------
    float ratio = rs / R0;                     // 计算电阻比率 RS/R0
    float ppm_nh3 = NH3_A * pow(ratio, NH3_B); // 幂函数公式计算 PPM
    sensorData.NH3 = ppm_nh3;

    // ---------------- 功能二：计算通用空气质量等级 (直观判断) ----------------
    // 为了兼容你之前的逻辑，我们将 ESP32的 4095 映射回 Arduino的 1023 比例
    // 这样你可以继续使用之前的 sensitivity 参数
    int raw_adc = analogRead(MQ135_PIN);
    int normalized_val = map(raw_adc, 0, 4095, 0, 1023);
    int aq_index = normalized_val * sensitivity / 1023;
    sensorData.air_quality_index = aq_index;

    String quality_label = interpret_air_quality(aq_index);
    sensorData.air_quality = quality_label;

    // ---------------- 串口打印结果 ----------------
    // Serial.print("[NH3 Sensor] ");
    // Serial.print("PPM: ");
    // Serial.print(ppm_nh3, 3); // 保留3位小数
    // Serial.print(" \t| ");

    // Serial.print("[Air Quality] ");
    // Serial.print("Index: ");
    // Serial.print(aq_index);
    // Serial.print(" -> ");
    // Serial.println(quality_label);
}

// ================= 辅助逻辑 =================

// 判断空气质量等级
String interpret_air_quality(int sensor_value)
{
    if (sensor_value < 50)
        return "Excellent (优)";
    else if (sensor_value < 100)
        return "Good (良)";
    else if (sensor_value < 150)
        return "Moderate (中)";
    else if (sensor_value < 200)
        return "Poor (差)";
    else
        return "Dangerous (危险)";
}

// 读取传感器电阻 RS
float readSensorResistance()
{
    long adcValue = 0;
    for (int i = 0; i < 20; i++)
    { // 采样20次取平均
        adcValue += analogRead(MQ135_PIN);
        delay(5);
    }
    adcValue = adcValue / 20;
    if (adcValue == 0)
        adcValue = 1;

    float voltage = (float)adcValue * V_REF / ADC_RES;
    // 反推电阻: RS = (Vin - Vout) / Vout * RL
    // 假设 Vin 为 5.0V
    float rs = ((5.0 - voltage) / voltage) * RL_VALUE;
    return rs;
}

// 校准 R0 (假设当前空气洁净)
float calibrateR0()
{
    float val = 0;
    for (int i = 0; i < 50; i++)
    {
        val += readSensorResistance();
        delay(20);
    }
    val = val / 50;
    // 洁净空气中系数约为 3.6
    return val / 3.6;
}

/*****************************************************************************************************/

/**************************************灯 风扇 水泵 加湿器 舵机等********************************************************/

int ledPin = 0;
int pumpPin = 0;
int humidityPin = 0;
int fanPin = 0;
int servoPin = 0;

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

    if (sensorData.sun == 1)
    {
        digitalWrite(ledPin, HIGH);
    }
    else
    {
        digitalWrite(ledPin, LOW);
    }

    if (sensorData.pump == 1)
    {
        digitalWrite(pumpPin, HIGH);
    }
    else
    {
        digitalWrite(pumpPin, LOW);
    }

    if (sensorData.humidity == 1)
    {
        digitalWrite(humidityPin, HIGH);
    }
    else
    {
        digitalWrite(humidityPin, LOW);
    }

    if (sensorData.fan == 1)
    {
        digitalWrite(fanPin, HIGH);
    }
    else
    {
        digitalWrite(fanPin, LOW);
    }

    if (sensorData.servo == 1)
    {
        digitalWrite(servoPin, HIGH);
    }
    else
    {
        digitalWrite(servoPin, LOW);
    }

    delay(1000);
}

/*****************************************************************************************************/

void printData()
{
    Serial.print("温度: ");
    Serial.print(sensorData.temperature);
    Serial.print("C\t湿度: ");
    Serial.print(sensorData.humidity);
    Serial.print(" %\t");
    Serial.print("CO2: ");
    Serial.print(sensorData.co2);
    Serial.print(" ppm\t");
    Serial.print("CO: ");
    Serial.print(sensorData.co);
    Serial.print(" ppm");
    Serial.print("\t");
    Serial.print("NH3: ");
    Serial.print(sensorData.NH3);
    Serial.print(" ppm\t");
    Serial.print("空气质量: ");
    Serial.print(sensorData.air_quality);
    Serial.print("(");
    Serial.print(sensorData.air_quality_index);
    Serial.print(")");
    Serial.print("\t光照强度: ");
    Serial.print(sensorData.sun);
    Serial.println();
    delay(300);
}



void setup()
{

    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    SGP30_Init();
    MQ7_init();
    MQ135_Init();
    Light_init();
}

void loop()
{
    DHT11_Read();
    SGP30_Read();
    MQ7_READ();
    MQ135_read();
    Light_read();
    printData();

    if (!client.connected())
    {
        reconnect();
    }
    client.loop(); // 必须调用以处理 MQTT 消息和维护连接

    // 定时发布消息
    unsigned long now = millis();
    if (now - lastMsg > interval)
    {
        lastMsg = now;
        // 生成一个随机数作为模拟传感器数据
        int value = random(0, 100);
        String payload = String(value);
        Serial.print("Publish message: ");
        // Serial.println(payload);

        // // 发布到主题
        // client.publish(pub_topic, payload.c_str());

        PublishMessage();
    }
}

