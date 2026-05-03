# -*- encoding: utf-8 -*-
'''
@File    :   config.py
@Author  :   编程学习园地 
@License :   该项目受专利、软著保护，仅供个人学习使用，严禁倒卖，一经发现，编程学习园地团队有必要追究法律责任！！！
'''


#该类用于配置模型
from pathlib import Path
import sys

#获取当前文件的绝对路径
file_path = Path(__file__).resolve()

#获取当前文件的上一级目录的路径
root_path = file_path.parent

#如果当前文件的父目录不在搜索路径中则添加进去
if root_path not in sys.path:
    sys.path.append(str(root_path))

#获取当前项目(工作目录)的相对路径
ROOT = root_path.relative_to(Path.cwd())
#数据源
SOURCES_LIST = ["图像", "视频","摄像头"]

# 模型路径配置
DETECTION_MODEL_DIR = ROOT / 'weights'

"""
注意：如果你想要加载自己训练的模型(yolov3、v5、v6、v7、v8及其微调版本,这些都支持,修改了网络结构的不支持)
需要在 DETECTION_MODEL_LIST 添加自己的模型名称(例如我要添加的模型文件名称为aa.pt,则在 DETECTION_MODEL_LIST 添加 aa.pt)
之后把模型放在weights目录下
"""
#侧边栏模型选择列表
DETECTION_MODEL_LIST = [
    "best.pt"]
