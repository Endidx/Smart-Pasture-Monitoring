/**
 * 智慧家居控制系统 - 核心逻辑 (Refactored)
 */

// ================= 配置区域 =================
const MQTT_CONFIG = {
  host: "190.92.241.121",
  port: 9001,
  clientId: `Web_Client_${Math.floor(Math.random() * 10000)}`,
  user: "",
  pass: "",
  useSSL: false,
  keepAlive: 60,
  cleanSession: true,
  reconnectTimeout: 5000,
};

const TOPICS = {
  SENSOR: "esp32/pub",
  CONTROL: "esp32/sub",
  intention: "home/living/control/upload",
};

// 全局状态管理
const DEVICE_STATE = {
  light: 0, // 灯光开关状态 (0=关, 1=开)
  // door: 0, // 门锁状态 (0=锁, 1=开)
  servo: 0, // 伺服电机状态 (0=关, 1=开)
  pump: 0, // 水泵状态 (0=关, 1=开)
  fan: 0, // 风扇状态 (0=关, 1=开)
  cam_power: 0, // 摄像头电源状态 (0=关, 1=开)
  device_humidity: 0,  // ← 新增：加湿器设备状态（0=关, 1=开）
  humidity: 0, // 当前湿度值 (单位: %)
  temperature: 0, // 当前温度值 (单位: ℃)
  co2: 0, // 当前CO2浓度 (单位: ppm)
  NH3: 0, // 当前NH3浓度 (单位: ppm)
  co: 0, // 当前CO浓度 (单位: ppm)
  temperature_threshold: 25, // 温度阈值 (单位: ℃)
  humidity_threshold: 10, // 湿度阈值 (单位: %)
  control_mode: 0, // "0" = 手动, "1" = 自动
};

/**
 * MQTT 客户端实例
 * 全局变量，存储 Paho.MQTT.Client 连接对象
 * @type {Paho.MQTT.Client|null}
 */
let client = null;
let servoAutoCloseTimer = null;
// ================= 核心连接功能 =================

/**
 * 初始化 MQTT 连接。
 *
 * 利用 Paho.MQTT.Client 连接到配置中的 broker，
 * 并注册连接丢失与消息到达的回调。
 * 连接成功后会订阅传感器主题，连接失败或断开时
 * 会按照 reconnectTimeout 重试。
 */

/**
 * 初始化 MQTT 连接
 *
 * 功能说明：
 * 1. 创建 Paho.MQTT.Client 实例
 * 2. 注册连接丢失回调 (onConnectionLost)
 * 3. 注册消息到达回调 (onMessageArrived)
 * 4. 配置连接参数并发起连接
 * 5. 连接成功后订阅传感器主题
 * 6. 连接失败时自动重试
 *
 * 消息分发逻辑：
 * - 若消息包含 devices 或 light 字段 → 设备状态同步
 * - 否则 → 传感器数据更新
 *
 * @function initMQTT
 * @returns {void}
 */
function initMQTT() {
  // 打印连接尝试日志 (带颜色样式)
  console.log(
    `%c[MQTT] 尝试连接：${MQTT_CONFIG.host}:${MQTT_CONFIG.port}`,
    "color: #2196F3; font-weight: bold;",
  );

  // 创建 MQTT 客户端实例
  client = new Paho.MQTT.Client(
    MQTT_CONFIG.host,
    Number(MQTT_CONFIG.port),
    MQTT_CONFIG.clientId,
  );

  // --- 连接丢失回调 ---
  client.onConnectionLost = (res) => {
    // 更新 UI 状态为离线
    updateStatus(false);
    // 非正常断开时自动重连
    if (res.errorCode !== 0) {
      console.warn(`[MQTT] 连接丢失：${res.errorMessage}`);
      setTimeout(initMQTT, MQTT_CONFIG.reconnectTimeout);
    }
  };

  // --- 消息到达回调 ---
// 第 95 行左右修改
client.onMessageArrived = (msg) => {
  const { payloadString: payload } = msg;
  try {
    const data = JSON.parse(payload);
    // console.log(`📩 数据到达:`, data);

    // 情况 A：收到的是传感器数据 (通常直接在根节点)
    if (data.temperature !== undefined || data.humidity !== undefined) {
      updateSensorUI(data); 
    }

    // 情况 B：收到的是设备状态同步 (通常在 devices 字段或包含开关字段)
    const deviceData = data.devices || data;
    if (deviceData.light !== undefined || deviceData.device_humidity !== undefined) {
      syncDeviceStates(deviceData);
    }

  } catch (e) { console.warn("解析失败:", payload); }
};

  // --- 连接配置选项 ---
  const options = {
    timeout: 5, // 连接超时时间 (秒)
    keepAliveInterval: MQTT_CONFIG.keepAlive,
    cleanSession: MQTT_CONFIG.cleanSession,
    useSSL: MQTT_CONFIG.useSSL,
    onSuccess: () => {
      console.log(
        "%c✔ [MQTT] 已连接成功!",
        "color: #4CAF50; font-weight: bold;",
      );
      updateStatus(true); // 更新 UI 为在线
      client.subscribe(TOPICS.SENSOR, { qos: 1 }); // 订阅传感器主题
    },
    onFailure: (e) => {
      console.error("❌ [MQTT] 连接失败:", e.errorMessage);
      updateStatus(false);
      setTimeout(initMQTT, MQTT_CONFIG.reconnectTimeout); // 失败后重试
    },
  };

  // 添加认证信息 (如果配置了)
  if (MQTT_CONFIG.user) options.userName = MQTT_CONFIG.user;
  if (MQTT_CONFIG.pass) options.password = MQTT_CONFIG.pass;

  // 发起连接
  try {
    client.connect(options);
  } catch (e) {
    console.error("MQTT Connect Error:", e);
  }
}

// ================= 状态同步与控制 =================

/**
 * 切换控制模式（手动/自动）
 *
 * 功能说明：
 * 1. 更新 DEVICE_STATE 中的模式状态
 * 2. 切换按钮视觉样式
 * 3. 发送 MQTT 消息同步模式到服务器
 * 4. 自动模式下可根据传感器数据自动触发设备
 *
 * @function setControlMode
 * @param {string} mode - 模式类型 ("0" 或 "1")
 * @returns {void}
 */
function setControlMode(mode) {
  if (![0, 1].includes(mode)) return;

  // 更新状态
  DEVICE_STATE.control_mode = mode;

  // 更新按钮样式
  const btn0 = document.getElementById("btn-0");
  const btn1 = document.getElementById("btn-1");

  if (btn0 && btn1) {
    if (mode === 0) {
      btn0.classList.add("active");
      btn1.classList.remove("active");
      console.log("🔧 已切换至 [手动控制] 模式");
    } else {
      btn1.classList.add("active");
      btn0.classList.remove("active");
      console.log("🤖 已切换至 [自动控制] 模式");
    }
  }

  // 发送 MQTT 消息同步模式
  sendMqttMessage({ control_mode: mode });

  // 自动模式下触发一次自动检测
  if (mode === 1) {
    ControlCheck();
    console.log("111自动模式下触发检测");
  }
}

/**
 * 切换控制模式（手动/自动）
 * @param {HTMLElement} btnElement - 被点击的按钮元素
 */
function toggleControlMode(btnElement) {
  // 切换状态 (0=手动，1=自动)
  const newMode = DEVICE_STATE.control_mode === 0 ? 1 : 0;
  DEVICE_STATE.control_mode = newMode;

  // 更新按钮显示
  const stateEl = btnElement.querySelector(".control-state");
  if (stateEl) {
    stateEl.innerText = newMode === 0 ? "手动" : "自动";
  }

  // 更新按钮激活状态
  btnElement.classList.toggle("active", newMode === 1);

  // ========== 新增：切换到手动模式时关闭所有设备 ==========
  if (newMode === 0) {
    const updates = {};
    const devicesToReset = ['light', 'servo', 'pump', 'fan', 'device_humidity'];
    
    devicesToReset.forEach(device => {
      if (DEVICE_STATE[device] === 1) {
        updates[device] = 0;
        DEVICE_STATE[device] = 0;
        updateDeviceUI(device, 0);
      }
    });
    
    if (Object.keys(updates).length > 0) {
      sendMqttMessage(updates);
      console.log("🔧 切换手动模式，已关闭所有设备:", updates);
    }
  }
  // =====================================================

  // 发送 MQTT 消息同步模式
  sendMqttMessage({ control_mode: newMode });

  // 自动模式下触发检测
  if (newMode === 1) {
    ControlCheck();
    console.log("🤖 已切换至 [自动控制] 模式");
  } else {
    console.log("🔧 已切换至 [手动控制] 模式");
  }
}

/**
 * 自动控制逻辑检查
 *
 * 功能说明：
 * 在自动模式下，根据传感器数据自动触发设备
 * 例如：温度过高自动开风扇，湿度过高自动开水泵
 *
 * @function 1ControlCheck
 * @returns {void}
 */
// 替换原来的 ControlCheck() 函数（第 180-220 行）
// 第 235 行左右重写
function ControlCheck() {
  // 1. 权限检查：非自动模式严禁下发指令
  if (DEVICE_STATE.control_mode !== 1) return;

  const updates = {};

  // --- 逻辑 1：光照控制灯光 ---
  const targetLight = (DEVICE_STATE.sun === 1) ? 1 : 0; // 1为暗则开灯
  if (DEVICE_STATE.light !== targetLight) {
    updates.light = targetLight;
  }

  // --- 逻辑 2：温度控制风扇 ---
  const targetFan = (DEVICE_STATE.temperature > DEVICE_STATE.temperature_threshold) ? 1 : 0;
  if (DEVICE_STATE.fan !== targetFan) {
    updates.fan = targetFan;
  }

  // --- 逻辑 3：湿度控制加湿器 (核心修改) ---
  // 使用传感器 humidity 比较阈值，更新 device_humidity 开关
  const targetHumidifier = (DEVICE_STATE.humidity < DEVICE_STATE.humidity_threshold) ? 1 : 0;
  if (DEVICE_STATE.device_humidity !== targetHumidifier) {
    updates.device_humidity = targetHumidifier;
  }

  // 2. 执行更新
  if (Object.keys(updates).length > 0) {
    // 同步本地状态，防止在下一帧数据回来前重复触发
    Object.assign(DEVICE_STATE, updates);
    
    // 更新 UI 视觉
    Object.keys(updates).forEach(key => updateDeviceUI(key, updates[key]));
    
    // 发送 MQTT 指令
    sendMqttMessage(updates);
    console.log("🤖 自动模式下发指令:", updates);
  }
}

/**
 * 统一发送控制函数
 * @param {Object} dataPayload 需要发送的局部或全部数据
 */
/**
 * 统一的 MQTT 发布函数。
 *
 * @param {Object} dataPayload 仅包含需要更新的设备字段。
 *
 * 该函数会：
 * 1. 检查客户端连接状态；
 * 2. 合并全局 DEVICE_STATE 与传入 payload 形成完整状态；
 * 3. 在控制台打印发送数据；
 * 4. 将 JSON 字符串封装为 Paho.Message 并发布到控制主题。
 */
/**
 * 完整状态同步发送函数
 * 每次操作都会把 5 个设备的最新状态全部发送给硬件，确保同步
 */
function sendMqttMessage(dataPayload) {
  if (!client || !client.isConnected()) return;

  // 1. 提取当前网页端记录的所有设备开关状态
  const allDeviceStates = {
    light: DEVICE_STATE.light,
    servo: DEVICE_STATE.servo,
    pump: DEVICE_STATE.pump,
    fan: DEVICE_STATE.fan,
    device_humidity: DEVICE_STATE.device_humidity,
    control_mode: DEVICE_STATE.control_mode // 同步控制模式
  };

  // 2. 合并当前所有状态和本次触发的最新指令 (dataPayload)
  const fullData = {
    timestamp: Date.now(),
    devices: { ...allDeviceStates, ...dataPayload },
  };

  const payload = JSON.stringify(fullData);
  const message = new Paho.MQTT.Message(payload);
  message.destinationName = TOPICS.CONTROL;
  message.qos = 1;

  try {
    client.send(message);
    console.log("📤 [状态全同步] 已发送完整数据包:", fullData.devices);
  } catch (e) {
    console.error("MQTT 发送异常:", e);
  }
}

/**
 * 控制按钮点击处理器
 *
 * 功能说明：
 * 1. 根据按钮当前状态翻转设备状态
 * 2. 立即更新 UI (提升交互响应感)
 * 3. 发送 MQTT 消息同步到服务器
 *
 * @function publishControl
 * @param {string} device - 设备名称 (对应 DEVICE_STATE 的键)
 * @param {HTMLElement} btnElement - 被点击的按钮元素
 * @returns {void}
 *
 * @example
 * // HTML: <button onclick="publishControl('light', this)">灯光</button>
 */
function publishControl(device, btnElement) {
  // 1. 切换本地状态 (根据按钮是否已有 active 类)
  DEVICE_STATE[device] = btnElement.classList.contains("active") ? 0 : 1;

  // 2. 立即更新 UI (先反馈，后发送，提升用户体验)
  updateDeviceUI(device, DEVICE_STATE[device]);

  // 3. 发送网络请求
  sendMqttMessage({ [device]: DEVICE_STATE[device] });
}

/**
 * 阈值滑块改变处理函数
 *
 * 功能说明：
 * 1. 解析滑块值为整数
 * 2. 更新全局状态
 * 3. 实时更新页面数字显示
 * 4. 发送 MQTT 消息同步阈值
 *
 * @function updateThreshold
 * @param {string} type - 阈值类型 (如 temperature_threshold, humidity_threshold)
 * @param {string|number} value - 滑块当前值
 * @returns {void}
 *
 * @example
 * // HTML: <input type="range" onchange="updateThreshold('temperature_threshold', this.value)">
 */
function updateThreshold(type, value) {
  const val = parseInt(value);
  DEVICE_STATE[type] = val;

  // 1. 实时更新页面数字显示 (增强反馈)
  const displayId = `val-${type.replace("_", "-")}`; // 如 val-temperature-threshold
  const displayEl = document.getElementById(displayId);
  if (displayEl) displayEl.innerText = val;

  // 2. 发送 MQTT 消息
  sendMqttMessage({ [type]: val });
  console.log(`📏 阈值已调整完毕：${type} -> ${val}`);
}

// ================= UI 更新逻辑 =================
/**
 * 更新 MQTT 连接状态指示器
 *
 * 功能说明：
 * 根据连接状态改变状态灯颜色和文字
 *
 * @function updateStatus
 * @param {boolean} isOnline - 当前是否在线
 * @returns {void}
 *
 * @requires HTML 元素：#mqttStatusDot (状态灯), #mqttStatusText (状态文字)
 */
function updateStatus(isOnline) {
  const dot = document.getElementById("mqttStatusDot");
  const text = document.getElementById("mqttStatusText");
  if (!dot || !text) return;

  dot.style.backgroundColor = isOnline ? "#4CAF50" : "#F44336"; // 绿/红
  text.innerText = isOnline ? "在线 (Connected)" : "离线 (Offline)";
}

/**
 * 用传感器数据更新 UI 显示
 *
 * 功能说明：
 * 根据预定义映射关系，将传感器数据填入对应页面元素
 *
 * @function updateSensorUI
 * @param {Object} data - 传感器数据对象
 * @property {number} data.temperatureerature - 温度值
 * @property {number} data.humiditydity - 湿度值
 * @property {number} data.co2 - CO2 浓度
 * @property {number} data.NH3 - NH3 浓度
 * @property {number} data.co - CO 浓度
 * @returns {void}
 *
 * @requires HTML 元素：#val-temperature, #val-humidity, #val-co2, #val-nh3, #val-co
 */
// 第 400 行左右修改
function updateSensorUI(data) {
  // 补齐 co2, NH3, co 的映射，键名必须与硬件发送的 JSON 一致
  const mappings = {
    temperature: { id: "val-temperature", unit: "℃" },
    humidity: { id: "val-humidity", unit: "%" },
    sun: { id: "val-sun", unit: "" },
    co2: { id: "val-co2", unit: "" }, // 对应 index.html 中的 id
    NH3: { id: "val-nh3", unit: "" }, // 注意 NH3 在 JSON 中是大写
    co: { id: "val-co", unit: "" }
  };

  Object.keys(mappings).forEach((key) => {
    const el = document.getElementById(mappings[key].id);
    if (el && data[key] !== undefined) {
      // 存入状态机供逻辑判断使用
      DEVICE_STATE[key] = data[key];
      
      // 更新文字
      if (key === "sun") {
        el.innerText = data[key] === 0 ? "亮" : "暗";
      } else {
        // 保持你要求的 Number 取整逻辑
        el.innerHTML = `${Number(data[key]).toFixed(0)}<small>${mappings[key].unit}</small>`;
      }
    }
  });

  // 每次传感器数据更新时，如果处于自动模式，执行一次检查
  if (DEVICE_STATE.control_mode === 1) {
    ControlCheck();
  }
}

/**
 * 同步服务器下发的设备状态到本地和 UI
 *
 * 功能说明：
 * 1. 遍历新状态对象
 * 2. 更新 DEVICE_STATE
 * 3. 根据设备类型更新对应 UI (滑块/按钮)
 *
 * @function syncDeviceStates
 * @param {Object} newStates - 服务器下发的设备状态对象
 * @returns {void}
 */
function syncDeviceStates(newStates) {
  Object.keys(newStates).forEach((device) => {
    if (DEVICE_STATE.hasOwnProperty(device)) {
      const oldState = DEVICE_STATE[device];  // 添加这一行
      DEVICE_STATE[device] = newStates[device];

      // ================= 新增：Servo 自动关闭逻辑 =================
      if (device === "servo" && newStates[device] === 1 && oldState !== 1) {
        // 清除之前的定时器（防止重复）
        if (servoAutoCloseTimer) {
          clearTimeout(servoAutoCloseTimer);
        }

        // 设置 10 秒后自动关闭
        servoAutoCloseTimer = setTimeout(() => {
          console.log("⏰ Servo 10 秒超时，自动关闭");
          DEVICE_STATE.servo = 0;
          updateDeviceUI("servo", 0);
          sendMqttMessage({ servo: 0 });
        }, 5000); // 10 秒

        console.log("🦾 Servo 已开启，10 秒后自动关闭");
      }

      // 阈值类设备 → 更新滑块和数字显示
      if (device.includes("threshold")) {
        const slider = document.querySelector(`input[oninput*="${device}"]`);
        const display = document.getElementById(
          `val-${device.replace("_", "-")}`,
        );
        if (slider) slider.value = newStates[device];
        if (display) display.innerText = newStates[device];
      } else {
        // 开关类设备 → 更新按钮状态
        updateDeviceUI(device, newStates[device]);
      }
    }
  });
}

/**
 * 更新单个控制按钮的视觉状态
 *
 * 功能说明：
 * 1. 设置按钮 active 类
 * 2. 更新状态文字 (不同设备有不同文字)
 * 3. 如果是摄像头则调用 handleCamera
 *
 * @function updateDeviceUI
 * @param {string} device - 设备名称
 * @param {number|boolean} state - 当前状态 (0/1 或 false/true)
 * @returns {void}
 */
function updateDeviceUI(device, state) {
  const isActive = state === 1 || state === true;
  // 通过 onclick 属性查找对应按钮
  const btn = document.querySelector(`[onclick*="'${device}'"]`);

  if (btn) {
    btn.classList.toggle("active", isActive);
    const stateEl = btn.querySelector(".control-state");
    if (stateEl) {
      // 设备特定状态文字映射
      const texts = {
        mode: isActive ? "1" : "0",
        cam_power: isActive ? "RECORDING" : "OFF",
        servo: isActive ? "WORKING" : "WAITING",
        pump: isActive ? "PUMPING" : "STOP",
      };
      stateEl.innerText = texts[device] || (isActive ? "ON" : "OFF");
    }
  }

  // 摄像头特殊处理
  if (device === "cam_power") handleCamera(isActive);
}

/**
 * 处理摄像头画面显示逻辑
 *
 * 功能说明：
 * 1. 切换容器样式类
 * 2. 更新状态文字和颜色
 * 3. 控制视频播放/暂停
 *
 * @function handleCamera
 * @param {boolean} isOn - 摄像头是否开启
 * @returns {void}
 *
 * @requires HTML 元素：#videoContainer, #monitorVideo, #camStatusText
 */
function handleCamera(isOn) {
  const container = document.getElementById("videoContainer");
  const video = document.getElementById("monitorVideo");
  const statusText = document.getElementById("camStatusText");
  if (!container || !video) return;

  container.classList.toggle("live-active", isOn);
  if (statusText) {
    statusText.innerText = isOn ? "CAM_01 [LIVE]" : "CAM_01 [OFFLINE]";
    statusText.style.color = isOn ? "#ff5252" : "white";
  }
  isOn ? video.play() : video.pause();
}

// ================= 生命周期 =================

/**
 * 页面加载完成时的入口函数。
 *
 * 初始化 MQTT 连接，并在短暂延迟后将各设备控件
 * 更新为默认状态，以防元素尚未渲染。
 */
window.onload = () => {
  initMQTT();
  // 延迟初始化 UI
  setTimeout(() => {
    Object.keys(DEVICE_STATE).forEach((d) =>
      updateDeviceUI(d, DEVICE_STATE[d]),
    );
  }, 500);
};
