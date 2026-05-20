# paper_assistant — 当前总结

这份文档不是 README,是**复盘 + 学习清单 + 当前固件说明**。下次你自己拿到一块新板子、或者想改这个表盘的某个细节,翻这里就能找到要看什么、改什么、怎么验证。

当前代码已经完成到:卡通表盘 + PCF85063 RTC + 局部刷新 + WiFiManager 配网 + NTP 自动校时。代码已通过 PlatformIO 编译,但还需要烧录到板子做硬件验证。

---

## 一、当前做到了什么

- 屏幕:200x200 黑白 e-paper,Waveshare ESP32-S3-Touch-ePaper-1.54 V2
- 显示:卡通形象(180x180,你给的灰度图转 1-bit)+ 时间 `HH:MM`(24pt 大字)
- 层叠效果:时间在卡通脑袋顶部那一窄条露头,卡通+影子作为整体盖住底层时间
- 刷新:开机 full refresh + 防鬼影震荡;平时每分钟 partial refresh 只刷上方 60px;每 30 次 partial 后 full refresh 清鬼影
- 电源:启动最早期拉高 GPIO17 `BAT_Control`,让锂电池供电在松开 PWR 后保持
- RTC:板载 PCF85063,通过 I2C 读写时间,用 RAM byte + OS bit 双重判断是否可信
- 联网:WiFiManager 管理 WiFi 凭据,首次没有凭据时开 `PaperAssist-AP` 配网 portal
- 校时:NTP 三级 fallback(`ntp.aliyun.com` -> `cn.pool.ntp.org` -> `time.windows.com`)
- 时间源链:NTP 成功就刷新 ESP32 system time 并写回 RTC;没网就用 RTC;RTC 无效才用编译时间兜底
- 时区:Asia/Shanghai,POSIX TZ 字符串为 `CST-8`

当前还没做/待验证:
- NTP + WiFiManager 需要烧录后用真实 WiFi 验证
- 触控 FT6336 未启用
- SHTC3 温湿度未接入
- 电池电量未显示
- 深睡眠省电未做
- partial LUT 还没换 Waveshare 官方版本,如果灰雾明显再优化

---

## 二、项目目录速查

```
paper_assistant/
├── platformio.ini              <- 板子型号 / Flash / PSRAM / 库依赖
├── src/                        <- PlatformIO 默认编译这里所有 .cpp
│   ├── main.cpp                <- setup() + loop(),总流程:RTC、屏幕、NTP、刷新调度
│   ├── home_screen.h/.cpp      <- render_home_full/partial + 配网提示渲染
│   ├── rtc_pcf85063.h/.cpp     <- PCF85063 RTC 直接寄存器读写
│   ├── net_time.h/.cpp         <- WiFiManager + NTP + 写回 RTC
│   ├── cartoon.h               <- 自动生成,卡通像素数据 (180x180 1-bit)
│   └── cartoon_mask.h          <- 自动生成,卡通形状蒙版
├── tools/
│   └── img2bitmap.py           <- 灰度图 -> 1-bit C 头文件转换脚本
├── assets/
│   ├── cartoon_src.jpg         <- 卡通原图(灰度版)
│   ├── cartoon_mask_final.png  <- 当前生效的 mask 预览
│   ├── previews/               <- 早期风格对比
│   └── previews_v2/            <- v2A/B/C 风格对比
└── lulu.jpg                    <- 原始素材
```

常用命令:

```bash
# 编译
/home/futureoo/.platformio/penv/bin/pio run

# 烧录
/home/futureoo/.platformio/penv/bin/pio run -t upload

# 串口监视
/home/futureoo/.platformio/penv/bin/pio device monitor
```

每次重新生成卡通用下面预设之一,**复制整条命令到终端跑**。所有预设共用 v3 mask(`--mask-cutoff 195 --mask-dilate 2`,你说“最好”的那个),只是 cartoon 风格不同。

### 预设 A — 经典抖动(密集颗粒、保留中灰)

对应 `assets/previews/A_dither_bg220.png` 的视觉,但用 180+fit 适配当前 200x200 屏。

```bash
python3 tools/img2bitmap.py assets/cartoon_src.jpg \
  --out src/cartoon.h --name cartoon \
  --size 180 --dither floyd --bg-cutoff 220 \
  --fit \
  --preview assets/cartoon_styleA.png \
  --mask-out src/cartoon_mask.h --mask-cutoff 195 --mask-dilate 2 \
  --mask-preview assets/cartoon_mask_final.png
```

### 预设 v2A — 柔和抖动(模糊软化、立体感强)

对应 `assets/previews_v2/v2A_smooth.png`。

```bash
python3 tools/img2bitmap.py assets/cartoon_src.jpg \
  --out src/cartoon.h --name cartoon \
  --size 180 --dither floyd --bg-cutoff 220 \
  --contrast 1.5 --blur 0.6 --fit \
  --preview assets/cartoon_final.png \
  --mask-out src/cartoon_mask.h --mask-cutoff 195 --mask-dilate 2 \
  --mask-preview assets/cartoon_mask_final.png
```

### 预设 v2B — 高对比块状(主体偏纯黑、利落)

对应 `assets/previews_v2/v2B_crisp.png`。

```bash
python3 tools/img2bitmap.py assets/cartoon_src.jpg \
  --out src/cartoon.h --name cartoon \
  --size 180 --dither floyd --bg-cutoff 230 \
  --contrast 2.0 --blur 0.0 --fit \
  --preview assets/cartoon_final.png \
  --mask-out src/cartoon_mask.h --mask-cutoff 195 --mask-dilate 2 \
  --mask-preview assets/cartoon_mask_final.png
```

### 切换预设的工作流

1. 复制对应预设命令到终端,跑
2. 看 `assets/cartoon_*.png` 预览满意吗
3. 不满意 -> 重选预设或微调参数(bg-cutoff / contrast / blur)再跑,不烧录
4. 满意 -> `/home/futureoo/.platformio/penv/bin/pio run -t upload` 烧到板子

关键原则: `src/cartoon.h` 已经被脚本覆盖了,只要不烧录就不会影响板子;烧之前总是先看 PNG 预览。

---

## 三、当前启动流程

```
ESP32 上电
  |
  v
GPIO17 BAT_Control 拉高,锁住锂电池供电
  |
  v
Serial + TZ=Asia/Shanghai(CST-8)
  |
  v
rtc.begin() -> I2C ACK 测试
  |
  +-- RTC 有效:rtc.readTime() -> settimeofday()
  |
  +-- RTC 无效/缺失:用编译时间 -> 如果 RTC 在线则 writeTime() 写回兜底时间
  |
  v
屏幕上电 + SPI init
  |
  v
防鬼影震荡:全黑 -> 全白 -> 全白
  |
  v
render_home_full() 先让用户看到当前最佳时间
  |
  v
开机 NTP:
  - WiFiManager 用已有凭据联网
  - 没凭据/连不上时开 PaperAssist-AP,屏幕底部显示提示
  - NTP 成功:settimeofday() + rtc.writeTime()
  - 如果分钟变化或进入过 portal,再 full refresh 清掉提示/更新时间
  |
  v
loop:
  - 每分钟 partial refresh 时间区域
  - 每 30 次 partial 后 full refresh 清鬼影
  - 每天 03:00 做一次 NTP,不打开 portal,失败就继续用 RTC
```

这个设计的核心是:

```
有权威源(NTP) -> 立刻刷新当前时间 + 写入 RTC
没权威源       -> 用上一次 NTP 写进 RTC 的时间继续走
RTC 也无效     -> 用编译时间,至少不要回到 1970
```

---

## 四、时间源系统:为什么这样设计

### 三层 fallback chain

| 层级 | 来源 | 什么时候用 | 可信度 | 是否持久 |
|---|---|---|---|---|
| 1 | NTP | 有 WiFi/互联网时 | 最高 | 成功后写回 RTC |
| 2 | PCF85063 RTC | 没网/开机早期 | 中高,会随晶振漂移 | 断电继续走 |
| 3 | 编译时间 | 首次烧录、RTC 无效 | 低,只是兜底 | 会写回 RTC,但等 NTP 覆盖 |

这就是“有条件刷新、没条件吃老本”:在线时拿权威时间;离线时使用上一次权威时间留下来的 RTC 结果。

### NTP 同步细节

- 首次配网 SSID:`PaperAssist-AP`
- 手机连上 AP 后访问 `192.168.4.1`,选择家庭 WiFi 并输入密码
- WiFiManager 会把凭据保存到 ESP32 NVS,重新烧录代码通常不会丢
- 开机同步允许 portal,超时 90 秒
- 每日 03:00 同步不允许 portal,只尝试已有凭据,避免半夜卡在配网界面
- NTP 成功后使用 `configTzTime("CST-8", ...)`,不能用 `configTime(0,0,...)`,否则 Arduino-ESP32 会把 TZ 重置成 UTC
- 每次 NTP 前重置 SNTP sync status,避免第二次同步误判为上一次已完成
- WiFi 用完后 `WiFi.disconnect(true, false)` + `WiFi.mode(WIFI_OFF)`,断开但不清凭据

---

## 五、PCF85063 RTC 是什么 / 怎么和它说话

### 为什么需要独立 RTC 芯片?

ESP32 自己有内置 RTC,但断电就丢。PCF85063 是独立芯片:
- 有自己的 32kHz 晶振 + 振荡器
- 断电时靠板上的备用电源继续走时
- 通过 I2C 让 ESP32 读/写时间

类比:ESP32 内置 RTC 像进程内变量;PCF85063 像写到本地存储,关机再开还在。

### I2C 通信回顾

I2C 只用两根线:SDA(数据)+ SCL(时钟),所有挂在总线上的芯片用 7-bit 地址区分。这块板子上挂了 3 个 I2C 设备共用同一对引脚:

| 设备 | 地址 | 引脚 |
|---|---|---|
| PCF85063 RTC | 0x51 | SDA=47, SCL=48 |
| FT6336 触控 | 0x38 | SDA=47, SCL=48 |
| SHTC3 温湿度 | 0x70 | SDA=47, SCL=48 |

Arduino-ESP32 用 `Wire` 库操作 I2C。基本读寄存器流程:

```cpp
Wire.beginTransmission(0x51);   // 我要跟地址 0x51 的设备说话
Wire.write(0x04);               // 我想从寄存器 0x04 开始读
Wire.endTransmission(false);    // false = 不发 stop,保持总线
Wire.requestFrom(0x51, 7);      // 读 7 个字节
while (Wire.available()) byte b = Wire.read();
```

### PCF85063 寄存器速查

| 地址 | 名字 | 关键 bit |
|---|---|---|
| 0x00 | Control_1 | bit 5 = STOP(必须 0 才计时),bit 1 = 12/24h(0=24h) |
| 0x03 | RAM_byte | 1 字节用户 RAM。我们写 `0x42` 标记“已设过有效时间” |
| 0x04 | Seconds | bit 7 = OS(振荡器停过电的标志),bits 6-0 = 秒(BCD) |
| 0x05 | Minutes | bits 6-0 = 分(BCD) |
| 0x06 | Hours | bits 5-0 = 时(BCD,24h 模式) |
| 0x07-0x0A | Days/Wday/Months/Years | BCD,年是 0-99 表示 2000-2099 |

### BCD 编码是什么?

Binary-Coded Decimal:每 4 bit 存 1 位十进制数。RTC 芯片普遍用,因为人读时不需要转换。

- 例:`59` 秒 -> BCD `0x59`,不是十六进制的 `0x3B`
- BCD -> 十进制:`(b >> 4) * 10 + (b & 0x0F)`
- 十进制 -> BCD:`((d / 10) << 4) | (d % 10)`

### 双重判定有效时间

OS=0(振荡器没停过电)不一定意味着时间正确,出厂寄存器可能是垃圾值。所以加了第二道:RAM_byte 写 `0x42` 作为“曾被 paper_assistant 写过”的指纹。

```
hasValidTime() = (RAM_byte == 0x42) AND (OS_bit == 0)
```

只有两条都满足才相信 RTC,否则 fallback 到编译时间。

---

## 六、Partial Refresh 是什么 / 为什么有点灰

| | Full refresh | Partial refresh |
|---|---|---|
| 用哪个 LUT | 全擦除 LUT,反复反转每个像素 | 差分 LUT,只翻转变化像素 |
| 耗时 | 约 1.3 秒 | 几百毫秒 |
| 视觉 | 整屏黑白闪烁 | 指定 window 内切换 |
| 对比度 | 纯黑/纯白 | 略低,可能有灰雾 |
| 鬼影 | 少 | 多次累积后明显 |

E-paper 的像素翻转靠电场脉冲控制黑/白颗粒移动。Full refresh 的脉冲更完整,所以慢且闪,但深黑纯白。Partial refresh 的脉冲更短,所以快、不闪,但翻转不够彻底,可能看起来灰。

当前对策:
- Partial window 只覆盖上方 60px,把灰雾限制在小范围
- 每 30 分钟左右做一次 full refresh 清鬼影
- 开机先做黑 -> 白 -> 白的防鬼影震荡

如果实物上灰雾不能接受,下一步是从 Waveshare BSP 提取官方 partial LUT 注入 GxEPD2。

---

## 七、踩过的坑 + 教训

### 1. PlatformIO 首次下载很大,中断不能续传

- 现象:`Downloading [###--] 5% ^C` 后再跑,从 0% 开始
- 教训:首次跑大约 10-20 分钟,不要 Ctrl+C
- 装好之后所有项目共用 `~/.platformio/packages/`,以后编译会快很多

### 2. Linux 没 dialout 组烧不进

- 现象:`Permission denied: '/dev/ttyACM0'`
- 修:`sudo usermod -aG dialout $USER` 然后重启或重新登录
- 临时方案:`sudo chmod 666 /dev/ttyACM0`

### 3. EPD_PWR 是 active-low

- 现象:烧录成功,Serial 输出完美,屏幕完全不动
- 根因:Waveshare BSP 里 `POWEER_EPD_ON()` 是 `gpio_set_level(pin, 0)`
- 教训:Waveshare 系列的电源 GPIO 极性要看官方 BSP 源码,不能靠默认假设

### 4. drawBitmap 三种用法容易混

| 调用 | bit=1 | bit=0 |
|---|---|---|
| `drawBitmap(x, y, bmp, w, h, BLACK)` | 画黑 | 透明 |
| `drawBitmap(x, y, bmp, w, h, BLACK, WHITE)` | 画黑 | 画白 |
| `drawBitmap(x, y, bmp, w, h, WHITE)` | 画白 | 透明 |

我们用第 1 种画 cartoon,第 3 种画 mask(擦白主体形状)。

### 5. mask 只负责遮罩底层,不负责裁切卡通

- 错误用法:用 mask 决定 cartoon 哪些像素能画,会导致影子被擦掉
- 正确用法:mask 只用来遮罩底层内容,不影响 cartoon 自身显示
- 当前生效:`cartoon.h` 完整(含影子),`cartoon_mask.h` 覆盖主体+影子整片形状

### 6. SSD1681 partial refresh 物理上偏灰

- 现象:partial 刷时间时,局部区域看上去灰雾
- 根因:partial LUT 的电压脉冲弱于 full LUT
- 当前对策:缩小 partial window + 定期 full refresh

### 7. 多次 partial 后会有鬼影

- 现象:背景泛灰,有上次画面残留
- 对策:setup 里防鬼影震荡;loop 里每 30 次 partial 后 full refresh

### 8. Arduino-ESP32 的 `configTime(0,0,...)` 会重置时区

- 现象:NTP 同步后本地时间可能变 UTC,再写回 RTC 就污染了 RTC
- 正确做法:用 `configTzTime("CST-8", server1, server2, server3)`
- 这个坑已经在 `src/net_time.cpp` 修掉

### 9. 锂电池供电需要代码拉住 BAT_Control

- 现象:断开 USB 后,按住 PWR 才有电,松手就断
- 根因:PWR 按键只是临时给系统上电;固件启动后必须尽早把 GPIO17 `BAT_Control` 拉高,让电源锁存保持
- 当前做法:`setup()` 一开始调用 `hold_battery_power()`,先于串口、RTC、屏幕和 WiFi
- 验证方式:烧录新版后,断开 USB,长按 PWR 约 1-2 秒等屏幕开始刷新/启动,松手后板子应继续运行

### 10. daily NTP 不应该打开配网 portal

- 开机允许配网,因为用户就在旁边
- 每日 03:00 校时不能打开 portal,否则无网时会卡在 AP 配网等待
- 当前做法:`portal_timeout_s=0` 时 `WiFiManager.setEnableConfigPortal(false)`

---

## 八、关键概念清单(你需要会的)

### 必会:改字号/位置/布局

Adafruit_GFX 坐标系:
- 原点 (0,0) 在屏幕左上,x 向右、y 向下
- 这块屏是 200x200,所有坐标都在 0-199
- `setRotation(0/1/2/3)` 旋转方向,当前用 0

字体高度 vs baseline:
- `setCursor(x, y)` 的 y 是文字 baseline,不是字符顶部
- 24pt 字符高度约 35px;baseline 设 50,字顶约 y=15

居中文字:

```cpp
display.getTextBounds(str, 0, baseline_y, &tx, &ty, &tw, &th);
int x = (200 - tw) / 2 - tx;
display.setCursor(x, baseline_y);
display.print(str);
```

### 建议会:改卡通效果

1-bit 图像打包:
- 每 byte 装 8 个像素,MSB 是最左边的像素
- 180x180 的图 = 23 字节/行 x 180 行 = 4140 字节
- `bit=1` 代表这里有内容,`bit=0` 代表空

抖动(dithering):
- Floyd-Steinberg 算法把灰度图模拟成 1-bit:用黑白点疏密制造灰度感
- 脚本会对原图先 contrast 增强 + blur 软化,再抖动

mask 的两类应用:
- 形状遮罩:mask 内的底层被擦白,卡通完整画上去。当前用这个
- 裁切:mask 外的 cartoon 像素被强制变白。不要和当前逻辑混用

### 暂时不必懂

- e-paper SSD1681 LUT 表 / 时序
- ESP32-S3 SPI master 驱动 / DMA
- GxEPD2 内部 buffer 管理 / firstPage/nextPage 多页机制
- ESP32 USB-CDC 枚举时序
- PSRAM 内存类型(QIO/OPI),partition 表

---

## 九、想自己改的话,改哪些参数

### 想改时间字号

`src/home_screen.cpp`:

```cpp
#include <Fonts/FreeSansBold24pt7b.h>
constexpr int TIME_BASELINE_Y = 50;
```

Adafruit_GFX 自带字体在 `.pio/libdeps/esp32s3_paper/Adafruit GFX Library/Fonts/`,有 9/12/18/24pt 的 FreeSans / FreeSerif / FreeMono 系列。

### 想改卡通位置

`src/home_screen.cpp`:

```cpp
constexpr int CARTOON_X = (SCR_W - cartoon_w) / 2;
constexpr int CARTOON_Y = 15;
```

### 想换卡通图

1. 把新图放到 `assets/cartoon_src.jpg` 或改命令里的路径
2. 跑生成命令
3. 看 PNG 预览
4. 编译烧录

### 想调卡通风格

| 参数 | 当前常用值 | 含义 | 调小 | 调大 |
|---|---|---|---|---|
| `--size` | 180 | 卡通输出尺寸 | 主体更小 | 主体更大,可能挤边 |
| `--bg-cutoff` | 220/230 | 灰度 >= 这值强制白 | 更多区域算背景 | 更多浅灰被保留 |
| `--contrast` | 1.5/2.0 | 抖动前对比度增强 | 主体偏灰、细节多 | 主体更纯黑 |
| `--blur` | 0.0/0.6 | 抖动前高斯模糊 px | 边缘锐利 | 边缘柔和 |
| `--mask-cutoff` | 195 | mask 灰度阈值 | mask 范围更小 | mask 范围更大 |
| `--mask-dilate` | 2 | mask 边缘外扩 px | 边缘紧贴主体 | 边缘外扩,补漏洞 |

### 想改时间格式 / 加日期

`src/home_screen.cpp` 的 `draw_time()`:

```cpp
snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
```

改成 12 小时制可以用 `%p %I:%M` 的思路,但当前是手写 `snprintf`,不是 `strftime`,要按 C 时间格式自己调整。

### 想改每日 NTP 时间

`src/main.cpp`:

```cpp
constexpr int DAILY_NTP_SYNC_HOUR = 3;
```

### 想改 NTP 服务器

`src/net_time.cpp`:

```cpp
static constexpr const char* NTP1 = "ntp.aliyun.com";
static constexpr const char* NTP2 = "cn.pool.ntp.org";
static constexpr const char* NTP3 = "time.windows.com";
```

---

## 十、烧录后验证清单

### RTC 验证

- [ ] monitor 看到 `[rtc] PCF85063 ACK ok`
- [ ] 第一次如果 RTC 无效,看到 `[clock] RTC invalid/missing -> fallback to build time` 和 `[rtc] wrote ...`
- [ ] 拔 USB 等 30 秒再插回
- [ ] 重启后看到 `[rtc] ram=0x42 (expect 0x42), OS=0 -> valid`
- [ ] 重启后的屏幕时间应该约等于拔掉前时间 + 离线秒数

### 电池供电验证

- [ ] 烧录新版后断开 USB
- [ ] 长按 PWR 约 1-2 秒启动
- [ ] 松开 PWR 后屏幕仍继续完成刷新,不会立刻断电
- [ ] 等下一分钟,屏幕时间能继续 partial refresh
- [ ] 如果松手仍断电,优先检查固件是否确实是新版,其次检查电池电压/插头极性

### NTP / WiFiManager 验证

- [ ] 第一次没 WiFi 凭据时,屏幕底部出现 `Setup WiFi: PaperAssist-AP`
- [ ] 手机连接 `PaperAssist-AP`,浏览器访问 `192.168.4.1`
- [ ] 配好 WiFi 后,monitor 看到 `[net] WiFi: connected`
- [ ] 看到 `[ntp] synced: ...`
- [ ] 同步后看到 `[rtc] wrote ...`,说明 NTP 结果写回 RTC
- [ ] 重启后不再要求配网,直接尝试已有凭据
- [ ] 断网重启时不会回到编译时间,而是用 RTC 时间继续走

### 刷新验证

- [ ] 等到分钟跳变,看到 `[tick] HH:MM -> partial #1`
- [ ] 屏幕只上方 60px 有变化,不是整屏闪
- [ ] 30 分钟后看到 `[tick] HH:MM -> FULL (anti-ghost)`
- [ ] 如果想加速验证,临时把 `FULL_REFRESH_EVERY_N_MIN = 3`

---

## 十一、小白学习路径(按优先级)

### Tier 1 — 必学,不会就只能照抄

1. C++ 基础概念:头文件 `.h` vs 实现 `.cpp`、`#include`、`constexpr`、template
2. Arduino 框架的 `setup()` / `loop()` 模型
3. GPIO 是什么:`pinMode`、`digitalWrite`、active-high vs active-low

### Tier 2 — 建议学,能独立做功能扩展

4. SPI / I2C 两种串行总线
   - SPI:CS/SCK/MOSI/MISO,屏幕用这个
   - I2C:SDA/SCL,RTC/触控/温湿度用这个
5. PlatformIO 怎么用
   - `platformio.ini` 是项目配置
   - `pio run` 编译,`pio run -t upload` 烧录,`pio device monitor` 看串口
   - 库源码在 `.pio/libdeps/`
6. GxEPD2 API
   - `display.init()`、`firstPage/nextPage`
   - `setFullWindow()` vs `setPartialWindow(x,y,w,h)`

### Tier 3 — 想真正掌握再学

7. e-paper 工作原理:LUT、全刷、局刷、鬼影
8. 图像处理基础:Pillow / OpenCV、阈值化、抖动、膨胀/腐蚀
9. 嵌入式 C++ 内存观念:Flash / RAM / PSRAM / PROGMEM
10. WiFiManager + NVS:为什么 WiFi 凭据重刷代码后还在
11. SNTP / POSIX TZ:为什么中国时区写作 `CST-8`,不是 `UTC+8`

---

## 十二、下一步候选

| 优先级 | 方向 | 工作量 | 备注 |
|---|---|---|---|
| 最高 | 烧录并验证电池保持 + RTC + WiFiManager + NTP | 20-40 分钟 | 当前代码已编译通过,需要实物确认 |
| 高 | 优化 partial LUT | 2-3 小时 | 如果实物上灰雾明显再做 |
| 中 | 接 SHTC3 温湿度 | 1 小时 | 卡通脚下小字显示 `°C / RH%` |
| 中 | 接 FT6336 触控 | 1-2 小时 | 点屏切表盘风格 / 显示更多状态 |
| 低 | 加电池电量显示 | 1 小时 | 需要确认 ADC 分压电路 |
| 低 | PWR 按键长按深睡眠 | 1 小时 | 真正做低功耗时再推进 |

最后一句:任何涉及卡通图像的改动,先看 PNG 预览,满意再烧录。任何涉及时间源的改动,先看串口日志里的 `[clock]`、`[rtc]`、`[net]`、`[ntp]` 四类输出。
