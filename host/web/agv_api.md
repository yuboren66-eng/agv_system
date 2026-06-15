---
title: 默认模块
language_tabs:
  - shell: Shell
  - http: HTTP
  - javascript: JavaScript
  - ruby: Ruby
  - python: Python
  - php: PHP
  - java: Java
  - go: Go
toc_footers: []
includes: []
search: true
code_clipboard: true
highlight_theme: darkula
headingLevel: 2
generator: "@tarslib/widdershins v4.0.30"

---

# 默认模块

Base URLs:

# Authentication

# Default

## POST assign请求

POST /api/assign

> Body 请求参数

```json
{
  "car": "C1",
  "target": "N4"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{
    "code": 200,
    "msg": "任务已成功指派",
    "data": {
        "car": "C1",
        "target": "N4"
    }
}
```

> 404 Response

```json
{
    "code": 404,
    "msg": "状态接口不存在",
    "data": null
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|
|404|[Not Found](https://tools.ietf.org/html/rfc7231#section-6.5.4)|none|Inline|

### 返回数据结构

状态码 **200**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» code|number|true|none||none|
|» msg|string|true|none||none|
|» data|object|true|none||none|
|»» car|string|true|none||none|
|»» target|string|true|none||none|

状态码 **404**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» code|number|true|none||none|
|» msg|string|true|none||none|
|» data|object|true|none||none|
|»» car|string|true|none||none|
|»» target|string|true|none||none|

## POST cancel请求

POST /api/cancel

> Body 请求参数

```json
{
  "car": "C1"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{
    "code": 200,
    "msg": "任务已成功取消",
    "data": {
        "car": "C1"
    }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|
|404|[Not Found](https://tools.ietf.org/html/rfc7231#section-6.5.4)|none|None|

### 返回数据结构

状态码 **200**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» code|number|true|none||none|
|» msg|string|true|none||none|
|» data|object|true|none||none|
|»» car|string|true|none||none|
|»» target|string|true|none||none|

## POST access请求

POST /api/access

> Body 请求参数

```json
{
  "edges": ["L2", "L8"],
  "nodes": ["N3"]
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{
    "code": 200,
    "msg": "已恢复通行",
    "data": {
        "edges": [
            "L2",
            "L8"
        ],
        "nodes": [
            "N3"
        ]
    }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|
|404|[Not Found](https://tools.ietf.org/html/rfc7231#section-6.5.4)|none|None|

### 返回数据结构

状态码 **200**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» code|number|true|none||none|
|» msg|string|true|none||none|
|» data|object|true|none||none|
|»» edges|string|true|none||none|
|»» nodes|string|true|none||none|

## POST repair请求

POST /api/repair

> Body 请求参数

```json
{
  "edges": ["L11"]
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{
    "code": 200,
    "msg": "修复任务已成功下发",
    "data": {
        "edges": [
            "L11"
        ]
    }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|
|404|[Not Found](https://tools.ietf.org/html/rfc7231#section-6.5.4)|none|None|

### 返回数据结构

状态码 **200**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» code|number|true|none||none|
|» msg|string|true|none||none|
|» data|object|true|none||none|
|»» edges|string|true|none||none|

## POST ban请求

POST /api/ban

> Body 请求参数

```json
{
  "edges": ["L12"],
  "nodes": ["N8"]
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{
    "code": 200,
    "msg": "禁止通行已生效",
    "data": {
        "edges": [
            "L12"
        ],
        "nodes": [
            "N8"
        ]
    }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|
|404|[Not Found](https://tools.ietf.org/html/rfc7231#section-6.5.4)|none|None|

### 返回数据结构

状态码 **200**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» code|number|true|none||none|
|» msg|string|true|none||none|
|» data|object|true|none||none|
|»» edges|string|true|none||none|
|»» nodes|string|true|none||none|

## GET status请求

GET /api/status

# AGV调度系统状态API

## 概述

此文档描述AGV调度系统状态API的请求和响应格式。前端通过此API获取系统实时状态。

## 端点

```
GET /api/status
```

## 请求

无需请求参数。

## 响应格式

响应为JSON格式，包含以下字段：

### 1. 系统基本信息

```json
{
  "timestamp": "2026-04-18 14:30:25",
  "system_status": "正常运行",
  "online_cars": 2,
  "task_count": 3,
  "broadcast": "[14:30] 系统自动刷新 · C2 正在执行任务(N3→N4) · L8 禁止通行"
}
```

### 2. 告警信息

```json
{
  "alerts": [
    {
      "level": "warn",
      "msg": "L8 路径封闭，绕行建议：L2→L6→L5"
    },
    {
      "level": "err", 
      "msg": "N5 节点通信中断"
    }
  ]
}
```

### 3. 小车状态

```json
"cars": {
    "C1": {
        "id": "C1",
        "name": "小车1",
        "status": "moving",
        "pos": "L1",
        "nxt": "N4",
        "path": [
            "L6",
            "L9",
            "L12"
        ],
        "task": null
    },
    "C2": {
        "id": "C2",
        "name": "小车2",
        "status": "moving",
        "pos": "L2",
        "nxt": "N5",
        "path": [
            "L7",
            "L12",
            "L15"
        ],
        "task": null
    }
}
```

**小车状态字段说明：**

- `status`: 小车状态，可选值：`idle`(空闲), `moving`(行进中), `fault`(故障), `offline`(离线)
- `pos`: 当前位置，格式为路径ID(如"L5")
- `nxt`:朝向节点号，格式为节点ID(如"N3")
- `task`: 当前任务，`null`表示无任务。行进中小车必须有任务信息。

### 4. 节点和路径状态

```json
{
  "nodes": {
    "N1": {"status": "idle"},
    "N2": {"status": "idle"},
    "N3": {"status": "fault"},
    "N4": {"status": "idle"},
    "N5": {"status": "occupied"}
  },
  "edges": {
    "L1": {"status": "idle"},
    "L2": {"status": "fault_temp"},
    "L3": {"status": "fault_repair"},
    "L4": {"status": "blocked"},
    "L5": {"status": "occupied"},
    "L6": {"status": "idle"}
  }
}
```

**状态说明：**

- 节点状态：`idle`(空闲), `occupied`(占用), `fault`(故障)
- 路径状态：`idle`(空闲), `occupied`(有车通过), `blocked`(禁止通行), `fault_temp`(暂时故障), `fault_repair`(待修复故障)

### 5. 拓扑信息（可选）

当系统拓扑结构发生变化时返回，用于动态更新界面：

```json
{
  "topology": {
    "nodes": [
      {
        "id": "N1",
        "label": "N1",
        "x": 80,
        "y": 80,
        "name": "仓储区"
      },
      {
        "id": "N2",
        "label": "N2", 
        "x": 240,
        "y": 80,
        "name": "分拣站A"
      }
    ],
    "edges": [
      {
        "id": "L1",
        "from": "N1",
        "to": "N2"
      },
      {
        "id": "L2",
        "from": "N2", 
        "to": "N5"
      }
    ]
  }
}
```

### 6. 日志追加信息（可选）

```json
{
  "logs": [
    {
      "level": "info",
      "msg": "C1 完成任务 T001",
      "timestamp": "14:30:22"
    },
    {
      "level": "warn",
      "msg": "L3 路径检测到异常",
      "timestamp": "14:29:15"
    }
  ]
}
```

## 完整响应示例

```json
{
    "timestamp": "2026-05-14 15:34:06",
    "system_status": "正常运行",
    "online_cars": 2,
    "task_count": 0,
    "broadcast": "[15:34:06] 系统运行中 · C1@N4(moving) · C2@N5(moving)",
    "alerts": [],
    "cars": {
        "C1": {
            "id": "C1",
            "name": "小车1",
            "status": "moving",
            "pos": "L1",
            "nxt": "N4",
            "path": [
                "L6",
                "L9",
                "L12"
            ],
            "task": null
        },
        "C2": {
            "id": "C2",
            "name": "小车2",
            "status": "moving",
            "pos": "L2",
            "nxt": "N5",
            "path": [
                "L7",
                "L12",
                "L15"
            ],
            "task": null
        }
    },
    "nodes": {
        "N1": {
            "status": "idle",
            "name": "N1"
        },
        "N2": {
            "status": "idle",
            "name": "N2"
        },
        "N3": {
            "status": "idle",
            "name": "N3"
        },
        "N4": {
            "status": "idle",
            "name": "N4"
        },
        "N5": {
            "status": "idle",
            "name": "N5"
        },
        "N6": {
            "status": "idle",
            "name": "N6"
        },
        "N7": {
            "status": "idle",
            "name": "N7"
        },
        "N8": {
            "status": "idle",
            "name": "N8"
        },
        "N9": {
            "status": "idle",
            "name": "N9"
        },
        "N10": {
            "status": "idle",
            "name": "N10"
        },
        "N11": {
            "status": "idle",
            "name": "N11"
        },
        "N12": {
            "status": "idle",
            "name": "N12"
        },
        "N13": {
            "status": "idle",
            "name": "N13"
        },
        "N14": {
            "status": "idle",
            "name": "N14"
        },
        "N15": {
            "status": "idle",
            "name": "N15"
        }
    },
    "edges": {
        "L1": {
            "status": "idle",
            "weight": "10",
            "from": "N1",
            "to": "N4"
        },
        "L2": {
            "status": "idle",
            "weight": "10",
            "from": "N2",
            "to": "N5"
        },
        "L3": {
            "status": "idle",
            "weight": "10",
            "from": "N3",
            "to": "N6"
        },
        "L4": {
            "status": "idle",
            "weight": "10",
            "from": "N4",
            "to": "N5"
        },
        "L5": {
            "status": "idle",
            "weight": "10",
            "from": "N5",
            "to": "N6"
        },
        "L6": {
            "status": "idle",
            "weight": "10",
            "from": "N4",
            "to": "N7"
        },
        "L7": {
            "status": "idle",
            "weight": "10",
            "from": "N5",
            "to": "N8"
        },
        "L8": {
            "status": "idle",
            "weight": "10",
            "from": "N6",
            "to": "N9"
        },
        "L9": {
            "status": "idle",
            "weight": "10",
            "from": "N7",
            "to": "N8"
        },
        "L10": {
            "status": "idle",
            "weight": "10",
            "from": "N8",
            "to": "N9"
        },
        "L11": {
            "status": "idle",
            "weight": "10",
            "from": "N7",
            "to": "N10"
        },
        "L12": {
            "status": "idle",
            "weight": "10",
            "from": "N8",
            "to": "N11"
        },
        "L13": {
            "status": "idle",
            "weight": "10",
            "from": "N9",
            "to": "N12"
        },
        "L14": {
            "status": "idle",
            "weight": "10",
            "from": "N10",
            "to": "N11"
        },
        "L15": {
            "status": "idle",
            "weight": "10",
            "from": "N11",
            "to": "N12"
        },
        "L16": {
            "status": "idle",
            "weight": "10",
            "from": "N10",
            "to": "N13"
        },
        "L17": {
            "status": "idle",
            "weight": "10",
            "from": "N11",
            "to": "N14"
        },
        "L18": {
            "status": "idle",
            "weight": "10",
            "from": "N12",
            "to": "N15"
        }
    },
    "logs": [],
    "meta": {
        "schema_version": "1.0",
        "host": "agv-dispatcher",
        "refresh_interval_ms": 2000
    }
}
```

## 注意事项

1. 所有时间戳使用24小时制，格式：`HH:MM:SS` 或 `YYYY-MM-DD HH:MM:SS`
2. 状态字段应为小写字符串
3. 当拓扑结构变化时，必须提供完整的`topology`数据
4. 前端收到拓扑数据后会更新显示，原有拓扑数据将被替换
5. `logs`字段用于追加日志，前端会将这些日志添加到日志面板中

在测试中，以下内容被删除，后续加上
```
{
    "type": "object",
    "properties": {
        "nodes": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "id": {
                        "type": "string"
                    },
                    "label": {
                        "type": "string"
                    },
                    "x": {
                        "type": "integer"
                    },
                    "y": {
                        "type": "integer"
                    },
                    "name": {
                        "type": "string"
                    }
                },
                "required": [
                    "id",
                    "label",
                    "x",
                    "y",
                    "name"
                ],
                "x-apifox-orders": [
                    "id",
                    "label",
                    "x",
                    "y",
                    "name"
                ]
            }
        },
        "edges": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "id": {
                        "type": "string"
                    },
                    "from": {
                        "type": "string"
                    },
                    "to": {
                        "type": "string"
                    }
                },
                "required": [
                    "id",
                    "from",
                    "to"
                ],
                "x-apifox-orders": [
                    "id",
                    "from",
                    "to"
                ]
            }
        }
    },
    "x-apifox-orders": [
        "nodes",
        "edges"
    ],
    "deprecated": true
}
```

> 返回示例

> 200 Response

```json
{
    "_comment": "AGV调度系统数据包 — 主机广播格式 v1.0 (适配完整拓扑)",
    "timestamp": "2026-04-17T14:45:00+08:00",
    "system_status": "正常运行",
    "online_cars": 2,
    "task_count": 3,
    "nodes": {
        "N1": {
            "status": "idle",
            "name": "仓储区",
            "description": "空闲"
        },
        "N2": {
            "status": "idle",
            "name": "分拣站A",
            "description": "空闲"
        },
        "N3": {
            "status": "idle",
            "name": "分拣站B",
            "description": "空闲"
        },
        "N4": {
            "status": "idle",
            "name": "装载区",
            "description": "空闲"
        },
        "N5": {
            "status": "idle",
            "name": "充电站",
            "description": "空闲"
        },
        "N6": {
            "status": "fault",
            "name": "充电站",
            "description": "故障"
        },
        "N7": {
            "status": "idle",
            "name": "充电站",
            "description": "空闲"
        },
        "N8": {
            "status": "idle",
            "name": "充电站",
            "description": "空闲"
        },
        "N9": {
            "status": "idle",
            "name": "充电站",
            "description": "空闲"
        },
        "N10": {
            "status": "idle",
            "name": "充电站",
            "description": "空闲"
        }
    },
    "edges": {
        "L1": {
            "status": "idle",
            "from": "N1",
            "to": "N2",
            "description": "空闲"
        },
        "L2": {
            "status": "idle",
            "from": "N2",
            "to": "N5",
            "description": "空闲"
        },
        "L3": {
            "status": "occupied",
            "from": "N2",
            "to": "N3",
            "description": "有车通过",
            "car_id": "C1"
        },
        "L4": {
            "status": "idle",
            "from": "N3",
            "to": "N6",
            "description": "空闲"
        },
        "L5": {
            "status": "occupied",
            "from": "N3",
            "to": "N4",
            "description": "有车通过",
            "car_id": "C2"
        },
        "L6": {
            "status": "idle",
            "from": "N4",
            "to": "N7",
            "description": "空闲"
        },
        "L7": {
            "status": "idle",
            "from": "N5",
            "to": "N8",
            "description": "空闲"
        },
        "L8": {
            "status": "blocked",
            "from": "N5",
            "to": "N6",
            "description": "禁止通行"
        },
        "L9": {
            "status": "idle",
            "from": "N6",
            "to": "N9",
            "description": "空闲"
        },
        "L10": {
            "status": "idle",
            "from": "N6",
            "to": "N7",
            "description": "空闲"
        },
        "L11": {
            "status": "fault",
            "from": "N7",
            "to": "N10",
            "description": "路径故障"
        },
        "L12": {
            "status": "idle",
            "from": "N8",
            "to": "N9",
            "description": "空闲"
        },
        "L13": {
            "status": "idle",
            "from": "N9",
            "to": "N10",
            "description": "空闲"
        }
    },
    "cars": {
        "C1": {
            "id": "C1",
            "name": "小车1",
            "status": "moving",
            "pos": "L3",
            "battery": 88,
            "speed": 1.2,
            "path": [
                "L2",
                "L8"
            ],
            "task": {
                "id": "T001",
                "from": "N1",
                "to": "N4",
                "priority": 1
            }
        },
        "C2": {
            "id": "C2",
            "name": "小车2",
            "status": "moving",
            "pos": "L5",
            "battery": 62,
            "speed": 1.0,
            "path": [
                "L3",
                "L4",
                "L6"
            ],
            "task": {
                "id": "T002",
                "from": "N3",
                "to": "N4",
                "priority": 2
            }
        }
    },
    "broadcast": "[14:45:00] 系统运行正常；C1 在L3上行驶；C2 在L5上行驶；L8禁止通行；L11故障；N6故障",
    "alerts": [
        {
            "level": "warn",
            "msg": "L8 路径已被手动禁止通行",
            "time": "14:30:11"
        },
        {
            "level": "err",
            "msg": "L11 路径故障，需要检修",
            "time": "14:35:22"
        },
        {
            "level": "err",
            "msg": "N6 充电站故障，无法使用",
            "time": "14:40:05"
        }
    ],
    "meta": {
        "schema_version": "1.0",
        "host": "dispatch-server-01",
        "refresh_interval_ms": 2000
    }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|
|404|[Not Found](https://tools.ietf.org/html/rfc7231#section-6.5.4)|none|None|

> [!NOTE]
>
> 下面内容原为自动生成，小车面向节点没有更新



### 返回数据结构

状态码 **200**

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|» timestamp|string|true|none||none|
|» system_status|string|true|none||none|
|» online_cars|integer|true|none||none|
|» task_count|integer|true|none||none|
|» broadcast|string|true|none||none|
|» alerts|[object]|false|none||none|
|»» level|string|true|none||none|
|»» msg|string|true|none||none|
|» cars|object|true|none||none|
|»» C1|object|true|none||none|
|»»» id|string|true|none||none|
|»»» name|string|true|none||none|
|»»» status|string¦null|true|none||none|
|»»» pos|string|true|none||none|
|»»» task|string¦null|false|none||none|
|»»» path|[string]|true|none||none|
|»» C2|object|true|none||none|
|»»» id|string|true|none||none|
|»»» name|string|true|none||none|
|»»» status|string|true|none||none|
|»»» pos|string|true|none||none|
|»»» task|string¦null|false|none||none|
|»»» path|[string]|true|none||none|
|» nodes|object|true|none||none|
|»» N1|object|true|none||none|
|»»» status|string|true|none||none|
|»» N2|object|true|none||none|
|»»» status|string|true|none||none|
|»» N3|object|true|none||none|
|»»» status|string|true|none||none|
|»» N4|object|true|none||none|
|»»» status|string|true|none||none|
|»» N5|object|true|none||none|
|»»» status|string|true|none||none|
|» edges|object|true|none||none|
|»» L1|object|true|none||none|
|»»» status|string|true|none||none|
|»» L2|object|true|none||none|
|»»» status|string|true|none||none|
|»» L3|object|true|none||none|
|»»» status|string|true|none||none|
|»» L4|object|true|none||none|
|»»» status|string|true|none||none|
|»» L5|object|true|none||none|
|»»» status|string|true|none||none|
|»» L6|object|true|none||none|
|»»» status|string|true|none||none|
|»» L7|object|true|none||none|
|»»» status|string|true|none||none|
|»» L8|object|true|none||none|
|»»» status|string|true|none||none|
|»» L9|object|true|none||none|
|»»» status|string|true|none||none|
|»» L10|object|true|none||none|
|»»» status|string|true|none||none|
|»» L11|object|true|none||none|
|»»» status|string|true|none||none|
|»» L12|object|true|none||none|
|»»» status|string|true|none||none|
|»» L13|object|true|none||none|
|»»» status|string|true|none||none|
|» logs|[object]|true|none||none|
|»» level|string|false|none||none|
|»» msg|string|false|none||none|
|»» timestamp|string|false|none||none|

#### 枚举值

|属性|值|
|---|---|
|system_status|正常运行|
|system_status|程序故障|
|system_status|暂停运行|
|level|warn|
|level|err|
|msg|提示信息1|
|msg|提示信息2|
|status|idle|
|status|moving|
|status|fault|
|status|offline|
|status|realloc|
|pos|N1|
|pos|N2|
|pos|N3|
|pos|L1|
|pos|L2|
|pos|L3|
|pos|L4|
|pos|L5|
|task|N1|
|task|N2|
|task|N3|
|status|idle|
|status|moving|
|status|fault|
|status|offline|
|status|realloc|
|pos|N1|
|pos|N2|
|pos|N3|
|pos|L1|
|pos|L2|
|pos|L3|
|pos|L4|
|pos|L5|
|task|N1|
|task|N2|
|task|N3|
|status|idle|
|status|occupied|
|status|fault|
|status|idle|
|status|occupied|
|status|fault|
|status|idle|
|status|occupied|
|status|fault|
|status|idle|
|status|occupied|
|status|fault|
|status|idle|
|status|occupied|
|status|fault|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|status|idle|
|status|occupied|
|status|blocked|
|status|fault_temp|
|status|fault_repair|
|level|info|
|level|warn|
|level|err|

# 数据模型

