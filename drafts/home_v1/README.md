# drafts/home_v1 — 首页草稿

## 文件
- `cartoon.h` — 150x150 1-bit 抖动版卡通头像 (Floyd-Steinberg + bg-cutoff 220)
- `home_screen.h/.cpp` — `render_home(display, now, full_redraw)` 的实现
- `main.cpp` — 顶层程序;`setup` 里用编译时间初始化时钟,`loop` 每分钟重绘

## 切换到正式 src/

Hello World (`src/main.cpp`) 验证通过后,执行:

```bash
cd /home/futureoo/Desktop/paper_assistant
cp drafts/home_v1/{cartoon.h,home_screen.h,home_screen.cpp} src/
mv drafts/home_v1/main.cpp src/main.cpp   # 覆盖原 Hello World
pio run -t upload
```

## 布局
```
y=0   ┌──────────────────────┐
      │                      │   ← 时间数字 (47-80 区间,大字号 24pt)
y=44  │   ╔══════════════╗   │
      │   ║              ║   │
      │   ║    卡通      ║   │  ← 卡通从 y=44 开始绘,覆盖时间数字下半
      │   ║   150x150    ║   │
      │   ║              ║   │
y=194 │   ╚══════════════╝   │
      │   2026/05/20 Wed     │   ← 日期 9pt
y=200 └──────────────────────┘
```

## TODO
- Task #6: `render_home(display, now, full_redraw=false)` 分支改用 `setPartialWindow` 只刷时间区域
- 加 PCF85063 硬件 RTC,断电保时间
- 接 WiFi NTP 校时
