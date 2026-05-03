# -*- encoding: utf-8 -*-
'''
@File    :   app.py
@Author  :   编程学习园地 
@License :   该项目受专利、软著保护，仅供个人学习使用，严禁倒卖，一经发现，编程学习园地团队有必要追究法律责任！！！
'''

#系统主类,系统程序的入口,在命令行中运行该类
from pathlib import Path
from PIL import Image
import streamlit as st
import config
from utils import load_model, infer_uploaded_image, infer_uploaded_video, infer_uploaded_webcam
import visualization

# 设置页面布局
st.set_page_config(
    page_title="Coding Learning Corner",
    page_icon="🤖",
    layout="wide",
    initial_sidebar_state="expanded"
    )

# 设置主标题
st.title("基于YOLOV8的牛行为检测系统")

# 侧边栏标题
st.sidebar.header("模型配置")

# 侧边栏-任务选择
task_type = st.sidebar.selectbox(
    "选择要进行的任务",
    ["目标检测", "数据分析"]
)

if task_type == "目标检测":
    # 目标检测模式
    model_type = None
    # 侧边栏-模型选择
    model_type = st.sidebar.selectbox(
        "选取模型",
        config.DETECTION_MODEL_LIST
    )

    #侧边栏-置信度
    confidence = float(st.sidebar.slider(
        "选取最小置信度", 10, 100, 25)) / 100

    model_path = ""
    if model_type:
        model_path = Path(config.DETECTION_MODEL_DIR, str(model_type))
    else:
        st.error("请在下拉框选择一个模型")
        st.stop()

    # 加载模型
    model = None
    try:
        model = load_model(model_path)
    except Exception as e:
        st.error(f"无法加载模型. 请检查路径: {model_path}")
        st.error(f"错误详情: {e}")
        st.stop()

    # 侧边栏-图像、视频、摄像头选择
    st.sidebar.header("图片/视频配置")
    source_selectbox = st.sidebar.selectbox(
        "选取文件类型",
        config.SOURCES_LIST
    )

    source_img = None
    if source_selectbox == config.SOURCES_LIST[2]: # 摄像头
        infer_uploaded_webcam(confidence,model)
    elif source_selectbox == config.SOURCES_LIST[0]: # 图像
        infer_uploaded_image(confidence, model)
    elif source_selectbox == config.SOURCES_LIST[1]: # 视频
        infer_uploaded_video(confidence, model)
    else:
        st.error("目前仅支持 '图片' '视频' '本地摄像头' ")

elif task_type == "数据分析":
    # 数据分析模式 - 显示可视化仪表板
    visualization.show_dashboard(embedded=True)
else:
    st.error("未知任务类型")