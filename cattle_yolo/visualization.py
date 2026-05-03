# -*- encoding: utf-8 -*-
'''
@File    :   visualization.py
@Author  :   编程学习园地
@License :   该项目受专利、软著保护，仅供个人学习使用，严禁倒卖，一经发现，编程学习园地团队有必要追究法律责任！！！
'''

import streamlit as st
import database
from datetime import datetime, timedelta
import pandas as pd
from pyecharts import options as opts
from pyecharts.charts import Bar, Line, Pie, Scatter
from streamlit_echarts import st_pyecharts
import numpy as np

def show_dashboard(embedded=False):
    """显示仪表板

    Args:
        embedded (bool): 是否为嵌入模式。如果为True，不显示主标题。
    """
    if not embedded:
        st.title("牛行为检测数据可视化")

    # 日期范围选择器
    col1, col2 = st.columns(2)
    with col1:
        # 嵌入式模式下使用更近的日期范围（最近1天），否则使用最近7天
        default_days = 1 if embedded else 7
        start_date = st.date_input("开始日期", value=datetime.now() - timedelta(days=default_days))
    with col2:
        end_date = st.date_input("结束日期", value=datetime.now())

    # 将日期转换为字符串格式
    start_str = start_date.strftime("%Y-%m-%d")
    end_str = end_date.strftime("%Y-%m-%d")

    # 获取统计数据
    behavior_stats = database.get_behavior_stats(start_str, end_str)
    daily_counts = database.get_daily_counts(start_str, end_str)
    recent_detections = database.get_detections(limit=100, start_date=start_str, end_date=end_str)

    # 显示概览卡片
    st.subheader("概览")
    col1, col2, col3 = st.columns(3)

    total_detections = sum([count for _, count in behavior_stats])
    with col1:
        st.metric("总检测次数", total_detections)
    with col2:
        unique_behaviors = len(behavior_stats)
        st.metric("行为类别数", unique_behaviors)
    with col3:
        avg_conf = 0
        if recent_detections:
            avg_conf = np.mean([det[5] for det in recent_detections])  # confidence在第6列
        st.metric("平均置信度", f"{avg_conf:.2%}")

    # 行为分布饼图
    st.subheader("行为分布")
    if behavior_stats:
        behaviors = [item[0] for item in behavior_stats]
        counts = [item[1] for item in behavior_stats]

        pie = (
            Pie()
            .add(
                "",
                [list(z) for z in zip(behaviors, counts)],
                radius=["30%", "75%"],
            )
            .set_global_opts(
                title_opts=opts.TitleOpts(title="行为分布图"),
                legend_opts=opts.LegendOpts(orient="vertical", pos_top="15%", pos_left="2%"),
            )
            .set_series_opts(label_opts=opts.LabelOpts(formatter="{b}: {c} ({d}%)"))
        )
        st_pyecharts(pie, height="500px")
    else:
        st.info("所选时间段内无检测数据")

    # 每日检测趋势图
    st.subheader("每日检测趋势")
    if daily_counts:
        # 将日期格式从 YYYY-MM-DD 转换为 MM-DD（去掉年份）
        dates = []
        for item in daily_counts:
            date_str = item[0]
            if '-' in date_str:
                # 格式：YYYY-MM-DD -> MM-DD
                parts = date_str.split('-')
                if len(parts) >= 3:
                    dates.append(f"{parts[1]}-{parts[2]}")
                else:
                    dates.append(date_str)
            else:
                dates.append(date_str)
        daily_nums = [item[1] for item in daily_counts]

        line = (
            Line()
            .add_xaxis(dates)
            .add_yaxis("检测数量", daily_nums, is_smooth=True)
            .set_global_opts(
                title_opts=opts.TitleOpts(title="每日检测趋势"),
                xaxis_opts=opts.AxisOpts(name="日期"),
                yaxis_opts=opts.AxisOpts(name="检测次数"),
                tooltip_opts=opts.TooltipOpts(trigger="axis"),
            )
        )
        st_pyecharts(line, height="400px")
    else:
        st.info("所选时间段内无每日检测数据")

    # 行为统计柱状图
    st.subheader("行为统计")
    if behavior_stats:
        behaviors = [item[0] for item in behavior_stats]
        counts = [item[1] for item in behavior_stats]

        bar = (
            Bar()
            .add_xaxis(behaviors)
            .add_yaxis("检测次数", counts)
            .set_global_opts(
                title_opts=opts.TitleOpts(title="各行为检测次数"),
                xaxis_opts=opts.AxisOpts(name="行为类别"),
                yaxis_opts=opts.AxisOpts(name="次数"),
            )
        )
        st_pyecharts(bar, height="400px")
    else:
        st.info("所选时间段内无行为统计数据")

    # 源类型分布
    st.subheader("检测来源分析")
    if recent_detections:
        # 统计不同源类型的数量
        source_types = {}
        for detection in recent_detections:
            source_type = detection[2]  # source_type在第3列
            source_types[source_type] = source_types.get(source_type, 0) + 1

        source_labels = list(source_types.keys())
        source_counts = list(source_types.values())

        if source_labels:
            pie2 = (
                Pie()
                .add(
                    "",
                    [list(z) for z in zip(source_labels, source_counts)],
                    radius=["40%", "75%"],
                )
                .set_global_opts(
                    title_opts=opts.TitleOpts(title="检测来源分布"),
                    legend_opts=opts.LegendOpts(orient="vertical", pos_top="15%", pos_left="2%"),
                )
                .set_series_opts(label_opts=opts.LabelOpts(formatter="{b}: {c} ({d}%)"))
            )
            st_pyecharts(pie2, height="400px")

    # 最近检测记录表格
    st.subheader("最近检测记录")
    if recent_detections:
        # 转换为DataFrame
        columns = ["ID", "时间戳", "源类型", "源名称", "行为", "置信度", "位置", "图片路径"]
        df = pd.DataFrame(recent_detections, columns=columns)

        # 格式化列
        df['时间戳'] = pd.to_datetime(df['时间戳'])
        df['置信度'] = df['置信度'].apply(lambda x: f"{x:.2%}")

        # 显示表格
        st.dataframe(
            df[['时间戳', '源类型', '行为', '置信度', '位置']],
            use_container_width=True,
            height=300
        )

        # 提供数据下载
        csv = df.to_csv(index=False).encode('utf-8')
        st.download_button(
            label="下载数据为CSV",
            data=csv,
            file_name=f"cattle_detections_{start_str}_to_{end_str}.csv",
            mime="text/csv",
        )
    else:
        st.info("所选时间段内无检测记录")

if __name__ == "__main__":
    show_dashboard()