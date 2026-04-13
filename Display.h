#ifndef DISPLAY_H
#define DISPLAY_H

/*
 * 初始化显示层。
 * 这个阶段会准备终端编码、进入备用屏幕、初始化日志和数据模块。
 */
void display_init(void);

/*
 * 退出显示层。
 * 这个阶段会恢复主屏幕和光标显示状态。
 */
void display_cleanup(void);

/*
 * 运行主事件循环。
 * 程序会不断渲染界面、读取按键、分发输入，直到用户选择退出。
 */
void display_run(void);

#endif
