
import base64
import os
import json
import time
from fastapi.responses import FileResponse
from openai import OpenAI
import dashscope
from fastapi import FastAPI, Request, Response, HTTPException
import paho.mqtt.client as mqtt
import threading
from dotenv import load_dotenv
import requests

load_dotenv()
app = FastAPI()

# ========== 全局 MQTT 长连接配置 ==========
_mqtt_client = None
_sensor_cache = {
    "data": None,
    "timestamp": 0,
    "lock": threading.Lock()
}
CACHE_DURATION = 60  # 缓存有效期 60 秒

def init_mqtt_long_connection(topic="esp32/pub", broker="190.92.241.121", port=1883):
    """
    初始化 MQTT 长连接客户端（后台持续运行）
    """
    global _mqtt_client

    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print(f"✓ MQTT 已连接到 {broker}")
            client.subscribe(topic)
            print(f"✓ 已订阅主题：{topic}")
        else:
            print(f"✗ MQTT 连接失败，错误码：{rc}")

    def on_message(client, userdata, msg):
        try:
            payload = msg.payload.decode()
            data = json.loads(payload)

            # 更新缓存数据
            with _sensor_cache["lock"]:
                _sensor_cache["data"] = {
                    "二氧化碳": data.get("co2", 0),
                    "一氧化碳": data.get("co", 0),
                    "氨气": data.get("NH3", 0),
                    "空气温度": data.get("temperature", 0),
                    "空气湿度": data.get("humidity", 0),
                }
                _sensor_cache["timestamp"] = time.time()

            # print(f"📡 [MQTT] 收到新数据 - CO2: {data.get('co2', 'N/A')}, 温度：{data.get('temperature', 'N/A')}°C")
        except Exception as e:
            print(f"处理 MQTT 消息时出错：{e}")

    def on_disconnect(client, userdata, rc):
        print(f"⚠ MQTT 断开连接，错误码：{rc}")
        # 尝试重连
        try:
            client.reconnect()
        except:
            pass

    # 创建客户端
    _mqtt_client = mqtt.Client()
    _mqtt_client.on_connect = on_connect
    _mqtt_client.on_message = on_message
    _mqtt_client.on_disconnect = on_disconnect

    try:
        _mqtt_client.connect(broker, port, 60)
        _mqtt_client.loop_start()

        # 等待连接建立（最多等 5 秒）
        for i in range(10):
            if _mqtt_client.is_connected():
                print("✓ MQTT 初始化完成")
                return True
            time.sleep(0.5)

        print("⚠ MQTT 连接超时")
        return False
    except Exception as e:
        print(f"MQTT 初始化失败：{e}")
        return False


def get_cached_sensor_data():
    """
    获取缓存的传感器数据（直接返回，无需等待）
    """
    with _sensor_cache["lock"]:
        current_time = time.time()

        # 如果有缓存且在有效期内，直接返回
        if _sensor_cache["data"] and (current_time - _sensor_cache["timestamp"]) < CACHE_DURATION:
            return _sensor_cache["data"]

    # 没有有效缓存，返回 None
    return None



def encode_audio(audio_path):
    with open(audio_path, "rb") as audio_file:
        return base64.b64encode(audio_file.read()).decode("utf-8")


# # 编码音频
# base64_audio = encode_audio("Recording.mp3")

def judgement(base64_audio):
    # 初始化 OpenAI 客户端（请确保 api_key 安全存储，不要硬编码在代码中）
    client = OpenAI(
        api_key=os.getenv("DASHSCOPE_API_KEY"),  # 建议改用环境变量
        base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
    )


    # # 编码音频
    # base64_audio = encode_audio("Recording.mp3")

    # 构造提示词：要求模型输出结构化意图判断 + 回复语
    system_instruction = """
    你是一个智能家居语音助手。请完成以下任务：
    1. 首先检测用户是否在语音中称呼了"小千"或"XiaoQian"作为唤醒词。
    2. 如果没有检测到唤醒词"小千"，则返回 intent 为 "no_wake_word"，response_text 为空字符串。
    3. 如果检测到唤醒词，则继续：
       - 听写用户语音内容。
       - 判断用户意图，意图类别包括：["open_fan", "close_fan", "open_light", "close_light", "open_pump", "close_pump", "open_humidity", 
       "close_humidity", "open_feeder", "close_feeder", "空气温度","空气湿度","一氧化碳","二氧化碳","氨气","other"]。
       - 根据意图生成简短自然的中文回复，例如："好的，已为你打开风扇"。
    4. 请以如下 JSON 格式返回结果（不要多余文字）：
    {
      "wake_word_detected": true 或 false,
      "intent": "意图类别（如果没有唤醒词则为 no_wake_word）",
      "response_text": "回复语句"
    }
    5. 请在文本中输出 JSON，但在语音回复中只说 response_text 的内容，不要读出 JSON 格式
    """


    text_content = ""

    completion = client.chat.completions.create(
        model="qwen3-omni-flash",
        messages=[
            {
                "role": "system",
                "content": system_instruction
            },
            {
                "role": "user",
                "content": [
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": f"data:;base64,{base64_audio}",
                            "format": "wav",  # 如果原始是 mp3，这里建议先转为 wav 或确认模型支持
                        },
                    },
                    {"type": "text", "text": "请分析这段音频并返回意图判断和回复。"},
                ],
            },
        ],
        modalities=["text"],  # 同时返回文本和语音
        # audio={"voice": "Cherry", "format": "wav"},
        stream=True,
        stream_options={"include_usage": True},
    )

    for chunk in completion:
        if chunk.choices:
            delta = chunk.choices[0].delta
            # 收集文本
            if hasattr(delta, "content") and delta.content:
                text_content += delta.content
            # 收集音频
            # if hasattr(delta, "audio") and delta.audio:
            #     print(delta)
            #     try:
            #         audio_string += delta.audio.get("data", "")
            #     except Exception as e:
            #         print(f"音频数据异常: {e}")
        else:
            # usage 信息
            if hasattr(chunk, "usage"):
                print(f"Token 使用情况: {chunk.usage}")

    # 解析意图 JSON（模型可能返回带 markdown 代码块，需清理）
    try:
        # 清理可能的 ```json ... ``` 包裹
        cleaned_text = text_content.strip()
        if cleaned_text.startswith("```json"):
            cleaned_text = cleaned_text[7:]
        if cleaned_text.endswith("```"):
            cleaned_text = cleaned_text[:-3]
        result = json.loads(cleaned_text.strip())
        wake_word_detected = result.get("wake_word_detected", False)

        # 如果没有检测到唤醒词，直接返回，不执行后续操作
        if not wake_word_detected:
            print("\n=== 未检测到唤醒词'小千'，忽略该请求 ===")
            return {
                "intent": "no_wake_word",
                "response_text": "",
                "wake_word_detected": False
            }

        intent = result.get("intent", "unknown")
        response_text = result.get("response_text", "抱歉，我没听清。")

        print(f"\n=== 意图识别结果 ===")
        print(f"意图：{intent}")
        print(f"回复：{response_text}")
        # audio_response = audioToText(response_text)
        return result
        # # 这里可以添加本地动作触发逻辑
        # if intent == "open_fan":
        #     print(">>> 执行操作：打开风扇")
        #     # gpio.setup(...); gpio.output(..., True) 等
        # elif intent == "close_fan":
        #     print(">>> 执行操作：关闭风扇")
        # # ... 其他意图处理

    except json.JSONDecodeError as e:
        print(f"JSON 解析失败: {e}")
        print(f"原始返回文本: {text_content}")
        intent = "parse_error"
        response_text = "抱歉，解析失败。"


def audioAPI(text):
    # 以下为北京地域url，若使用新加坡地域的模型，需将url替换为：https://dashscope-intl.aliyuncs.com/api/v1
    dashscope.base_http_api_url = 'https://dashscope.aliyuncs.com/api/v1'

    # text = "那我来给大家推荐一款T恤，这款呢真的是超级好看，这个颜色呢很显气质，而且呢也是搭配的绝佳单品，大家可以闭眼入，真的是非常好看，对身材的包容性也很好，不管啥身材的宝宝呢，穿上去都是很好看的。推荐宝宝们下单哦。"
    # SpeechSynthesizer接口使用方法：dashscope.audio.qwen_tts.SpeechSynthesizer.call(...)
    response = dashscope.MultiModalConversation.call(
        # 如需使用指令控制功能，请将model替换为qwen3-tts-instruct-flash
        model="qwen3-tts-flash",
        # 新加坡和北京地域的API Key不同。获取API Key：https://help.aliyun.com/zh/model-studio/get-api-key
        # 若没有配置环境变量，请用百炼API Key将下行替换为：api_key = "sk-xxx"
        api_key=os.getenv("DASHSCOPE_API_KEY"),
        text=text,
        voice="Cherry",
        language_type="Chinese",  # 建议与文本语种一致，以获得正确的发音和自然的语调。
        # 如需使用指令控制功能，请取消下方注释，并将model替换为qwen3-tts-instruct-flash
        # instructions='语速较快，带有明显的上扬语调，适合介绍时尚产品。',
        # optimize_instructions=True,
        stream=False
    )
    if response.status_code != 200:
        print(f"错误: {response.status_code}")
        print(response.text)
        return "抱歉，我无法处理你的请求。"
    elif response.status_code == 200:
        url = response["output"]["audio"]["url"]
        data = requests.get(url).content
        with open("audio_assistant_response.wav", "wb") as f:
            f.write(data)
        return url
        # print(data)
    else:
        print("错误:", response.text)
        return response
    # print(response)


@app.post("/intention")
def intention(base64_audio):
    audio_url = 0
    senser= get_cached_sensor_data()
    if senser is None:
        print("⚠ 未获取到传感器数据，使用默认值")
        senser = {
            "二氧化碳": 400,
            "一氧化碳": 0,
            "氨气": 0,
            "空气温度": 25,
            "空气湿度": 50
        }

    # if message is not None:
    #     message = json.loads(message)
    #     senser={
    #         "二氧化碳":message["co2"],
    #         "一氧化碳":message["co"],
    #         "氨气":message["NH3"],
    #         "空气温度":message["temperature"],
    #         "空气湿度":message["humidity"],
    #     }

    result=judgement(base64_audio)
    response_text = result.get("response_text")
    if result.get("intent") == "unknown" :
        response_text = "抱歉，我无法处理你的请求。"
    elif result.get("intent") == "二氧化碳":
        response_text = f"二氧化碳为{senser['二氧化碳']}ppm"
        audio_url = audioAPI(response_text)
    elif result.get("intent") == "一氧化碳":
        response_text = f"一氧化碳为{senser['一氧化碳']}ppm"
        audio_url = audioAPI(response_text)
    elif result.get("intent") == "氨气":
        response_text = f"氨气为{senser['氨气']}ppm"
        audio_url = audioAPI(response_text)
    elif result.get("intent") == "空气温度":
        response_text = f"空气温度为{senser['空气温度']}℃"
        audio_url = audioAPI(response_text)
    elif result.get("intent") == "空气湿度":
        response_text = f"空气湿度为{senser['空气湿度']}%"
        audio_url = audioAPI(response_text)
    elif len(response_text)>0:
    # audio_data=audioToText(response_text)
        audio_url=audioAPI(response_text)
    else:
        audio_url=0
    # 返回Base64编码的音频数据
    response = {
        "intent": result.get("intent", "unknown"),
        "response_text": response_text,
        "audio_url": audio_url
        # "audio_base64": base64.b64encode(audio_data).decode('utf-8') if audio_data else None
        # "audio_base64": audio_data if audio_data else None
    }
    # print(response)
    return response


@app.post("/upload")
async def upload(request: Request):
    """
    接收原始 PCM 数据，转换为 WAV 格式并保存
    """
    data = await request.body()
    if len(data) == 0:
        return Response(content="No Data", status_code=400)
    print(Response)

    # 假设 PCM 参数（根据你的实际情况调整）
    sample_rate = 16000  # 采样率
    channels = 1         # 单声道
    sample_width = 2     # 16bit = 2 bytes

    # 方法 1: 使用 wave 模块
    # import wave
    # with wave.open("Recording.wav", "wb") as wf:
    #     wf.setnchannels(channels)
    #     wf.setsampwidth(sample_width)
    #     wf.setframerate(sample_rate)
    #     wf.writeframes(data)

    # 方法 2: 使用 soundfile 模块（如果你已经安装了）
    # import numpy as np
    # import soundfile as sf
    # audio_np = np.frombuffer(data, dtype=np.int16)
    # sf.write("Recording.wav", audio_np, samplerate=sample_rate)

    base64_audio = base64.b64encode(data).decode('utf-8')

    # response=intention(base64_audio)
    # 将阻塞的音频处理放到线程池中执行，避免阻塞事件循环
    # await asyncio.to_thread(process_audio, data)

    # base64_audio = encode_audio("Recording.wav")
    info=intention(base64_audio)
    print(info)
    info=str(info)
    return Response(content=info, status_code=200)


@app.get("/{filename}")
async def get_audio(filename: str):
    """
    根据文件名返回音频文件
    """
    # filename = "audio_assistant_response.wav"
    file_path = os.path.join(filename)

    # 检查文件是否存在
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="音频文件不存在")

    # 根据文件扩展名推测媒体类型（可选）
    media_type = None
    if filename.endswith(".mp3"):
        media_type = "audio/mpeg"
    elif filename.endswith(".wav"):
        media_type = "audio/wav"
    elif filename.endswith(".ogg"):
        media_type = "audio/ogg"
    # 可以继续添加其他格式

    # 返回文件
    return FileResponse(path=file_path,
                        media_type=media_type,
                        headers={
                            "Content-Disposition": f"inline; filename=\"{filename}\""
                        })


# 可选：添加一个简单的首页
@app.get("/")
async def root():
    return {"message": "音频服务运行中，请访问 /audio/文件名 来获取音频文件"}

# def get_mqtt_message(topic, broker="broker.emqx.io", port=1883, timeout=10):
#     """
#     订阅 MQTT 主题，等待接收第一条消息后返回其内容。
#
#     参数:
#         topic (str): 要订阅的主题
#         broker (str): MQTT broker 地址，默认 audioAPI.mosquitto.org
#         port (int): broker 端口，默认 1883
#         timeout (int): 等待消息的超时时间（秒），默认 10
#
#     返回:
#         str 或 None: 成功收到消息返回消息内容（解码后的字符串），超时返回 None
#     """
#     # 创建一个队列，用于线程间传递消息
#     msg_queue = queue.Queue(maxsize=1)
#
#     # 连接成功回调
#     def on_connect(client, userdata, flags, rc):
#         if rc == 0:
#             print(f"Connected to {broker}, subscribing to {topic}")
#             client.subscribe(topic)
#         else:
#             print(f"Connection failed with code {rc}")
#             msg_queue.put(None)  # 通知主线程连接失败
#
#     # 收到消息回调
#     def on_message(client, userdata, msg):
#         print(f"Message received on {msg.topic}")
#         payload = msg.payload.decode()
#         msg_queue.put(payload)   # 将消息放入队列
#         client.disconnect()       # 收到消息后断开连接
#
#     # 创建客户端实例
#     client = mqtt.Client()
#     client.on_connect = on_connect
#     client.on_message = on_message
#
#     try:
#         # 连接到 broker
#         client.connect(broker, port, 60)
#     except Exception as e:
#         print(f"Connection error: {e}")
#         return None
#
#     # 启动网络循环（非阻塞）
#     client.loop_start()
#
#     try:
#         # 等待队列中有数据，超时时间为 timeout
#         result = msg_queue.get(timeout=timeout)
#         return result
#     except queue.Empty:
#         print(f"Timeout after {timeout} seconds, no message received.")
#         return None
#     finally:
#         # 停止循环并断开连接
#         client.loop_stop()
#         # 如果客户端仍然连接着，确保断开
#         if client.is_connected():
#             client.disconnect()
#

if __name__ == "__main__":
    # base64_audio = encode_audio("Recording.mp3")
    # intention(base64_audio)

    text="你好啊，我是智慧牧场助手，有什么可以帮你的？"
    # audioToText(text)
    audioAPI(text)




    # print("正在启动 MQTT 长连接...")
    # init_mqtt_long_connection("esp32/pub")
    #
    api_key = os.getenv("DASHSCOPE_API_KEY")
    # print(api_key)
    #
    #
    # import uvicorn
    # uvicorn.run(app, host="0.0.0.0", port=8788)



    # 调用函数，等待 audioAPI/topic 主题的消息
    # message = get_mqtt_message("esp32/pub", timeout=15)
    # if message is not None:
    #     message=json.loads(message)
    #     print(message["co2"])
    # else :
    #     print("Timeout.")

    # if message:
    #     print(f"Got message: {message}")
    # else:
    #     print("No message received.")



