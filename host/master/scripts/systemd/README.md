# AGV SystemD 服务管理

## 依赖关系

```
┌─────────────────┐
│  agv-log-daemon  │  ← 中央日志管道（最早启动，最后停止）
└────────┬─────────┘
         │ After
         ▼
┌─────────────────┐
│ agv-model-manager│  ← SHM 创建者 + 初始数据写入（地图 + 小车）
└────────┬─────────┘
         │ After + Requires
         ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  agv-router      │     │ agv-http-status  │     │  agv-http-topo  │
│  (路径查询)      │     │ (/api/status)    │     │ (/api/topo)     │
└────────┬─────────┘     └──────────────────┘     └──────────────────┘
         │ After                                           │
         ▼                                                  │
┌─────────────────┐     ┌─────────────────┐                │
│  agv-planner     │     │  agv-http-task  │                │
│  (任务规划)      │     │(/api/assign等)  │                │
└────────┬─────────┘     └──────────────────┘                │
         │                                                  │
         ▼                                                  │
┌─────────────────┐     ┌─────────────────┐                │
│agv-mqtt-subscriber│    │agv-mqtt-publisher│               │
│ (MQTT ← SHM)     │    │ (SHM → MQTT)    │               │
└──────────────────┘     └──────────────────┘               │
                                                             │
                ┌────────────────────────────────────────────┘
                ▼
     ┌──────────────────┐
     │  nginx + fcgiwrap │  ← HTTP 反向代理 / FCGI 调度
     └────────┬──────────┘
              │
              ▼
     ┌──────────────────┐
     │  前端 Web 页面    │  ← /var/www/ENavigate/web/index.html
     └──────────────────┘
```

## 启动顺序（安装后自动保证）

```
syslog/journal  →  agv-log-daemon
                →  agv-model-manager  (创建 SHM)
                →  agv-router, agv-http-*
                →  agv-planner
                →  agv-mqtt-*
```

## 安装

```bash
# 1. 复制 service 文件
sudo cp agv-log-daemon.service       /etc/systemd/system/
sudo cp agv-model-manager.service    /etc/systemd/system/
sudo cp agv-router.service           /etc/systemd/system/
sudo cp agv-planner.service          /etc/systemd/system/
sudo cp agv-http-status.service      /etc/systemd/system/
sudo cp agv-http-topo.service        /etc/systemd/system/
sudo cp agv-http-task.service        /etc/systemd/system/
sudo cp agv-mqtt-subscriber.service  /etc/systemd/system/
sudo cp agv-mqtt-publisher.service   /etc/systemd/system/
sudo cp agv.target                   /etc/systemd/system/

# 2. 配置 ExecStart 中的实际路径
#    如果编译产物不在 /opt/enavigate-master/build，修改 ExecStart 路径

# 3. 重新加载 systemd
sudo systemctl daemon-reload

# 4. 启用全部服务
sudo systemctl enable agv.target
sudo systemctl enable agv-log-daemon.service
sudo systemctl enable agv-model-manager.service
# ... 依次启用其他服务

# 5. 启动目标（会自动拉入所需服务）
sudo systemctl start agv.target
```

## 运行检查

```bash
# 查看所有 AGV 服务状态
systemctl list-units --type=service 'agv-*'

# 查看特定服务日志
journalctl -u agv-model-manager -f
journalctl -u agv-http-status -f
```

## 停止

```bash
sudo systemctl stop agv.target
```

## 重置共享内存

```bash
# 先停止所有 AGV 服务
sudo systemctl stop agv.target

# 运行重置程序
sudo /opt/enavigate-master/build/tests/reset_shm

# 重新启动
sudo systemctl start agv.target
```

## 路径说明

| 变量 | 默认值 | 说明 |
|---|---|---|
| `EXEC_PATH` | `/opt/enavigate-master/build` | 编译产物目录 |
| `WEB_DIR` | `/var/www/ENavigate/web` | 前端静态页面 |
| `MQTT_HOST` | `localhost:1883` | Mosquitto 地址 |

如果编译产物不在 `/opt/enavigate-master/build`，请修改各 service 文件的 `ExecStart`。
