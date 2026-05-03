# -*- encoding: utf-8 -*-
'''
@File    :   database.py
@Author  :   编程学习园地
@License :   该项目受专利、软著保护，仅供个人学习使用，严禁倒卖，一经发现，编程学习园地团队有必要追究法律责任！！！
'''

import sqlite3
import datetime
from pathlib import Path

DB_PATH = Path(__file__).parent / "cattle_detection.db"

def init_db():
    """初始化数据库，创建表"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    # 创建检测记录表
    c.execute('''
        CREATE TABLE IF NOT EXISTS detection_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME NOT NULL,
            source_type TEXT NOT NULL,  -- 'image', 'video', 'webcam'
            source_name TEXT,           -- 文件名或摄像头ID
            behavior TEXT NOT NULL,     -- 检测到的行为类别
            confidence REAL NOT NULL,   -- 置信度
            position TEXT,              -- 位置信息，如'x1,y1,x2,y2'
            image_path TEXT             -- 存储检测结果图片的路径（可选）
        )
    ''')
    # 创建索引以加速查询
    c.execute('CREATE INDEX IF NOT EXISTS idx_timestamp ON detection_log(timestamp)')
    c.execute('CREATE INDEX IF NOT EXISTS idx_behavior ON detection_log(behavior)')
    conn.commit()
    conn.close()

def insert_detection(source_type, source_name, behavior, confidence, position=None, image_path=None):
    """插入一条检测记录"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    timestamp = datetime.datetime.now().isoformat()
    c.execute('''
        INSERT INTO detection_log (timestamp, source_type, source_name, behavior, confidence, position, image_path)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    ''', (timestamp, source_type, source_name, behavior, confidence, position, image_path))
    conn.commit()
    conn.close()

def get_detections(limit=1000, start_date=None, end_date=None, behavior=None):
    """查询检测记录"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    query = 'SELECT * FROM detection_log WHERE 1=1'
    params = []
    if start_date:
        query += ' AND DATE(timestamp) >= ?'
        params.append(start_date)
    if end_date:
        query += ' AND DATE(timestamp) <= ?'
        params.append(end_date)
    if behavior:
        query += ' AND behavior = ?'
        params.append(behavior)
    query += ' ORDER BY timestamp DESC LIMIT ?'
    params.append(limit)
    c.execute(query, params)
    rows = c.fetchall()
    conn.close()
    return rows

def get_behavior_stats(start_date=None, end_date=None):
    """获取行为统计（每种行为的数量）"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    query = 'SELECT behavior, COUNT(*) as count FROM detection_log WHERE 1=1'
    params = []
    if start_date:
        query += ' AND DATE(timestamp) >= ?'
        params.append(start_date)
    if end_date:
        query += ' AND DATE(timestamp) <= ?'
        params.append(end_date)
    query += ' GROUP BY behavior ORDER BY count DESC'
    c.execute(query, params)
    rows = c.fetchall()
    conn.close()
    return rows

def get_daily_counts(start_date=None, end_date=None):
    """获取每日检测数量"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    query = '''
        SELECT DATE(timestamp) as date, COUNT(*) as count
        FROM detection_log
        WHERE 1=1
    '''
    params = []
    if start_date:
        query += ' AND DATE(timestamp) >= ?'
        params.append(start_date)
    if end_date:
        query += ' AND DATE(timestamp) <= ?'
        params.append(end_date)
    query += ' GROUP BY DATE(timestamp) ORDER BY date'
    c.execute(query, params)
    rows = c.fetchall()
    conn.close()
    return rows

# 初始化数据库
init_db()